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


def fetch_users(pgsql):
    cursor = pgsql['pg_birthday'].cursor()
    cursor.execute(
        """
        SELECT
            id,
            chat_id
        FROM birthday.users
        ORDER BY id
        """
    )
    return [
        {
            'id': row[0],
            'chat_id': row[1],
        }
        for row in cursor
    ]


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
            last_notification_time,
            user_id
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
            'user_id': row[6],
        }
        for row in cursor
    ]


@pytest.mark.pgsql(
    'pg_birthday',
    queries=[
        """
        INSERT INTO birthday.users(id, chat_id)
        VALUES (1000, 100500), (1001, 100501)
        """,
        """
        INSERT INTO birthday.birthdays(
            person,
            y,
            m,
            d,
            notification_enabled,
            user_id
        )
        VALUES ('name', 2000, 1, 1, true, 1000),
               ('name', 2000, 1, 2, true, 1000),
               ('name', 2000, 1, 2, true, 1001)
        """
    ],
)
@pytest.mark.parametrize(
    'sender_chat_id, expected_message, expect_deletion',
    [
        pytest.param(
            100500,
            'Deleted all your birthdays and forgot about you',
            True,
            id='done',
        ),
        pytest.param(
            100502,
            'You are not registered',
            False,
            id='already_unregistered',
        ),
    ]
)
@pytest.mark.now(_NOW.isoformat())
async def test_unregister(
    service_client,
    pgsql,
    mockserver,
    sender_chat_id: int,
    expected_message: str,
    expect_deletion: bool,
):
    # to update mocked time
    await service_client.invalidate_caches()

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getUpdates')
    def _handler_get_updates(request):
        # return new updates only once
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
                            'text': '/unregister',
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

    users = fetch_users(pgsql)
    birthdays = fetch_birthdays(pgsql)

    if expect_deletion:
        assert users == [
            {
                'id': 1001,
                'chat_id': 100501,
            },
        ]
        assert len(birthdays) == 1
    else:
        assert users == [
            {
                'id': 1000,
                'chat_id': 100500,
            },
            {
                'id': 1001,
                'chat_id': 100501,
            },
        ]
        assert len(birthdays) == 3
