import datetime as dt
from typing import Any
from typing import Dict
from typing import Optional

import pytest
import pytz

from testsuite.databases import pgsql

_TZ_MOSCOW = pytz.timezone('Europe/Moscow')
_NOW = _TZ_MOSCOW.localize(dt.datetime(2023, 3, 15, 12))

_TELEGRAM_TOKEN = 'fake_token'
_USAGE = 'Usage: /add_birthday DD.MM[.YYYY] Person Name'


def fetch_birthdays(pgsql):
    cursor = pgsql['pg_birthday'].cursor()
    cursor.execute(
        """
        SELECT
            person,
            y,
            m,
            d,
            notification_enabled,
            last_notification_time
        FROM birthday.birthdays
        ORDER BY id
        """
    )
    return [
        {
            'person': row[0],
            'year': row[1],
            'month': row[2],
            'day': row[3],
            'is_enabled': row[4],
            'last_notification_time': row[5],
        }
        for row in cursor
    ]


@pytest.mark.parametrize(
    'sender_chat_id, message, expected_message, expected_birthday',
    [
        pytest.param(
            100500,
            '/add_birthday 01.02.2003 KINIAEV Foma',
            'Inserted the birthday of KINIAEV Foma on 01.02',
            {
                'person': 'KINIAEV Foma',
                'year': 2003,
                'month': 2,
                'day': 1,
                'is_enabled': True,
                'last_notification_time': None,
            },
            id='ok',
        ),
        pytest.param(
            100500,
            '/add_birthday 01.02 KINIAEV Foma',
            'Inserted the birthday of KINIAEV Foma on 01.02',
            {
                'person': 'ЛШТШФУМ Ащьф',
                'year': None,
                'month': 2,
                'day': 1,
                'is_enabled': True,
                'last_notification_time': None,
            },
            id='without_year',
        ),
        pytest.param(
            100500,
            '/add_birthday@bot 01.02.2003 KINIAEV Foma',
            'Inserted the birthday of KINIAEV Foma on 01.02',
            {
                'person': 'KINIAEV Foma',
                'year': 2003,
                'month': 2,
                'day': 1,
                'is_enabled': True,
                'last_notification_time': None,
            },
            id='with_bot_tag',
        ),
        pytest.param(
            100500,
            '/add_birthday 01.023.2003 KINIAEV Foma',
            _USAGE,
            None,
            id='wrong_month',
        ),
        pytest.param(
            100500,
            '/add_birthday 013.02.2003 KINIAEV Foma',
            _USAGE,
            None,
            id='wrong_day',
        ),
        pytest.param(
            100500,
            '/add_birthday 013.02.20033 KINIAEV Foma',
            _USAGE,
            None,
            id='long_year',
        ),
        pytest.param(
            100500,
            '/add_birthday 013.02.233 KINIAEV Foma',
            _USAGE,
            None,
            id='short_year',
        ),
        pytest.param(
            100501,
            '/add_birthday 01.02.2003 KINIAEV Foma',
            'Not ready yet',
            None,
            id='unauthorized',
        ),
        pytest.param(
            100500,
            '/add_birthday 01.02.2003 ЛШТШФУМ Ащьф',
            'Inserted the birthday of ЛШТШФУМ Ащьф on 01.02',
            {
                'person': 'ЛШТШФУМ Ащьф',
                'year': 2003,
                'month': 2,
                'day': 1,
                'is_enabled': True,
                'last_notification_time': None,
            },
            id='cyrillic',
        ),
    ]
)
@pytest.mark.now(_NOW.isoformat())
async def test_add_birthday(
    service_client,
    pgsql,
    mockserver,
    sender_chat_id: int,
    message: str,
    expected_message: str,
    expected_birthday: Dict[str, Any],
):
    # to update mocked time
    await service_client.invalidate_caches()

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getUpdates')
    def _handler_get_updates(request):
        # return new updates only once
        # TODO account request params
        if _handler_get_updates.times_called < 2:
            return {
                'ok': True,
                'result': [
                    {
                        'update_id': 1,
                        'message': {
                            'message_id': 1,
                            'date': 1,
                            'chat': {
                                'id': sender_chat_id,
                                'type': 'private',
                            },
                            'text': message,
                        }
                    }
                ],
            }
        else:
            return {
                'ok': True,
                'result': [],
            }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/sendMessage')
    def handler_send_message(request):
        return {
            'ok': True,
            'description': 'foo',
            'result': {
                'message_id': 1,
                'date': 1,
                'chat': {
                    'id': sender_chat_id,
                    'type': 'private',
                },
                'text': 'Text',
            },
        }

    request = await handler_send_message.wait_call()
    request_data = request['request'].form
    request_text = request_data.pop('text').encode().decode('unicode-escape')
    assert request_text == expected_message
    assert request['request'].form == {
        'chat_id': sender_chat_id,
    }

    birthdays = fetch_birthdays(pgsql)
    if expected_birthday:
        birthdays = [expected_birthday]
    else:
        assert not birthdays
