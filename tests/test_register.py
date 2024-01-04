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


@pytest.mark.pgsql(
    'pg_birthday',
    queries=[
        """
        INSERT INTO birthday.users(id, chat_id)
        VALUES (1000, 100500)
        """,
    ],
)
@pytest.mark.parametrize(
    'sender_chat_id, expected_message, expected_users',
    [
        pytest.param(
            100500,
            'Already registered',
            [
                {
                    'id': 1000,
                    'chat_id': 100500,
                },
            ],
            id='already_registered',
        ),
        pytest.param(
            100501,
            'Done. Note that all your personal data will be stored in plain '
            'text. I promise not to look :)',
            [
                {
                    'id': 1,
                    'chat_id': 100501,
                },
                {
                    'id': 1000,
                    'chat_id': 100500,
                },
            ],
            id='done',
        ),
    ]
)
@pytest.mark.now(_NOW.isoformat())
async def test_register(
    service_client,
    pgsql,
    mockserver,
    sender_chat_id: int,
    expected_message: str,
    expected_users: List[Dict[str, Any]],
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
                            'text': '/register',
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
    assert users == expected_users
