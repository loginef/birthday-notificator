import datetime as dt
from typing import Any
from typing import Dict
from typing import List
from typing import Optional

import pytest
import pytz

from testsuite.databases import pgsql

_TZ_MOSCOW = pytz.timezone('Europe/Moscow')
_NOW = _TZ_MOSCOW.localize(dt.datetime(2023, 3, 15, 12))

_TELEGRAM_TOKEN = 'fake_token'
_MAX_RETURNED_ITEMS = 5


def insert_birthday(
    pgsql,
    person: str,
    month: int,
    day: int,
    is_enabled: bool,
    last_notification_time: Optional[dt.datetime] = None,
    year: Optional[int] = None
):
    cursor = pgsql['pg_birthday'].cursor()
    cursor.execute(
        """
        INSERT INTO birthday.birthdays(
            person,
            y,
            m,
            d,
            notification_enabled,
            last_notification_time
        )
        VALUES (%s, %s, %s, %s, %s, %s)
        """,
        (person, year, month, day, is_enabled, last_notification_time)
    )


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
    'birthdays_in_db, sender_chat_id, expected_message',
    [
        pytest.param(
            [
                dict(month=1, day=16),
                dict(month=1, day=17),
                dict(month=3, day=20),
                dict(month=4, day=17),
                dict(month=12, day=20),
            ],
            100500,
            'Next birthdays:\n'
            'person2 on 20.03\n'
            'person3 on 17.04\n'
            'person4 on 20.12\n'
            'person0 on 16.01\n'
            'person1 on 17.01',
            id='basic',
        ),
        pytest.param(
            [
                dict(month=1, day=15),
                dict(month=1, day=17),
                dict(month=3, day=15),
                dict(month=4, day=17),
                dict(month=12, day=20),
            ],
            100500,
            'Next birthdays:\n'
            'person2 on 15.03\n'
            'person3 on 17.04\n'
            'person4 on 20.12\n'
            'person0 on 15.01\n'
            'person1 on 17.01',
            id='include_today',
        ),
        pytest.param(
            [
                dict(month=1, day=16),
                dict(month=1, day=17),
                dict(month=2, day=17),
                dict(month=2, day=18),
                dict(month=2, day=19),
                dict(month=3, day=14),
                dict(month=3, day=15),
                dict(month=3, day=16),
                dict(month=12, day=20),
            ],
            100500,
            'Next birthdays:\n'
            'person6 on 15.03\n'
            'person7 on 16.03\n'
            'person8 on 20.12\n'
            'person0 on 16.01\n'
            'person1 on 17.01',
            id='too_many_events',
        ),
        pytest.param(
            [],
            100500,
            'There are no birthdays',
            id='no_events',
        ),
        pytest.param(
            [
                dict(month=1, day=16),
                dict(month=1, day=17),
                dict(month=3, day=20),
                dict(month=4, day=17),
                dict(month=12, day=20),
            ],
            100501,
            'No birthdays for you, sorry',
            id='wrong_sender',
        ),
    ]
)
@pytest.mark.now(_NOW.isoformat())
async def test_next_birthdays(
    service_client,
    pgsql,
    mockserver,
    birthdays_in_db: List[Dict[str, Any]],
    sender_chat_id: int,
    expected_message: str
):
    for i, event in enumerate(birthdays_in_db):
        insert_birthday(
            pgsql,
            person=f'person{i}',
            month=event['month'],
            day=event['day'],
            is_enabled=True,
            last_notification_time=_NOW,
            year=None,
        )

    # to update mocked time
    await service_client.invalidate_caches()

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getUpdates')
    def _handler_get_updates(request):
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
                        'text': '/next_birthdays',
                    }
                }
            ],
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
    assert request['request'].form == {
        'chat_id': sender_chat_id,
        'text': expected_message
    }
