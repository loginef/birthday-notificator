import base64
import json
import datetime as dt
from typing import Any
from typing import Dict
from typing import List
from typing import Optional

import pytest
import pytz

from testsuite.databases import pgsql

import button_pb2

_TZ_MOSCOW = pytz.timezone('Europe/Moscow')
_NOW = _TZ_MOSCOW.localize(dt.datetime(2023, 3, 15, 12))

_TELEGRAM_TOKEN = 'fake_token'
_MAX_RETURNED_ITEMS = 6
_CONTEXT_ID_NEXT_BDS = 0
_CONTEXT_ID_EDIT_BD = 1
_BUTTON_ID_EDIT_BD = 0
_BUTTON_ID_DELETE_BD = 1
_BUTTON_ID_CANCEL = 2


def insert_birthday(
    pgsql,
    person: str,
    month: int,
    day: int,
    is_enabled: bool,
    id: int,
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
            last_notification_time,
            id
        )
        VALUES (%s, %s, %s, %s, %s, %s, %s)
        """,
        (person, year, month, day, is_enabled, last_notification_time, id)
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
            last_notification_time,
            id
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
            'id': row[6],
        }
        for row in cursor
    ]


def reset_id_sequence(pgsql):
    cursor = pgsql['pg_birthday'].cursor()
    cursor.execute("SELECT setval('birthdays_id_seq', 1);")


class Button:
    def __init__(
        self,
        title: str,
        birthday_id: Optional[int],
        context_id: Optional[int],
        button_id: Optional[int]
    ):
        self.title = title
        self.data = self._to_proto(birthday_id, context_id, button_id)

    def _to_proto(
        self,
        birthday_id: Optional[int],
        context_id: Optional[int],
        button_id: Optional[int]
    ) -> str:
        button = button_pb2.ButtonData()
        if button_id is not None:
            button.button_type = button_id
        if context_id is not None:
            button.context = context_id
        if birthday_id is not None:
            button.bd_id = birthday_id
        return base64.b64encode(button.SerializeToString()).decode('utf-8')


@pytest.mark.parametrize(
    'birthdays_in_db, sender_chat_id, expected_message, expected_buttons',
    [
        pytest.param(
            [
                dict(month=1, day=16, id=1000),
                dict(month=1, day=17, id=1001),
                dict(month=3, day=20, id=1002),
                dict(month=4, day=17, id=1003),
                dict(month=12, day=20, id=1004),
            ],
            100500,
            'Next 5 birthdays:',
            [
                Button(
                    title='person2 on 20.03',
                    birthday_id=1002,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person3 on 17.04',
                    birthday_id=1003,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person4 on 20.12',
                    birthday_id=1004,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person0 on 16.01',
                    birthday_id=1000,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person1 on 17.01',
                    birthday_id=1001,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
            ],
            id='basic',
        ),
        pytest.param(
            [
                dict(month=1, day=15, id=1000),
                dict(month=1, day=17, id=1001),
                dict(month=3, day=15, id=1002),
                dict(month=4, day=17, id=1003),
                dict(month=12, day=20, id=1004),
            ],
            100500,
            'Next 5 birthdays:',
            [
                Button(
                    title='person2 on 15.03',
                    birthday_id=1002,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person3 on 17.04',
                    birthday_id=1003,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person4 on 20.12',
                    birthday_id=1004,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person0 on 15.01',
                    birthday_id=1000,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person1 on 17.01',
                    birthday_id=1001,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
            ],
            id='include_today',
        ),
        pytest.param(
            [
                dict(month=1, day=16, id=1000),
                dict(month=1, day=17, id=1001),
                dict(month=2, day=17, id=1002),
                dict(month=2, day=18, id=1003),
                dict(month=2, day=19, id=1004),
                dict(month=3, day=14, id=1005),
                dict(month=3, day=15, id=1006),
                dict(month=3, day=16, id=1007),
                dict(month=12, day=20, id=1008),
            ],
            100500,
            'Next 6 birthdays:',
            [
                Button(
                    title='person6 on 15.03',
                    birthday_id=1006,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person7 on 16.03',
                    birthday_id=1007,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person8 on 20.12',
                    birthday_id=1008,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person0 on 16.01',
                    birthday_id=1000,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person1 on 17.01',
                    birthday_id=1001,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
                Button(
                    title='person2 on 17.02',
                    birthday_id=1002,
                    context_id=_CONTEXT_ID_NEXT_BDS,
                    button_id=_BUTTON_ID_EDIT_BD
                ),
            ],
            id='too_many_events',
        ),
        pytest.param(
            [],
            100500,
            'There are no birthdays',
            None,
            id='no_events',
        ),
        pytest.param(
            [
                dict(month=1, day=16, id=1000),
                dict(month=1, day=17, id=1001),
                dict(month=3, day=20, id=1002),
                dict(month=4, day=17, id=1003),
                dict(month=12, day=20, id=1004),
            ],
            100501,
            'No birthdays for you, sorry',
            None,
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
    expected_message: str,
    expected_buttons: Optional[List[Button]],
):
    for i, event in enumerate(birthdays_in_db):
        insert_birthday(
            pgsql,
            person=f'person{i}',
            month=event['month'],
            day=event['day'],
            is_enabled=True,
            id=event['id'],
            last_notification_time=_NOW,
            year=None,
        )

    # to update mocked time
    await service_client.invalidate_caches()

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getMe')
    def _handler_get_me(request):
        return {
            'ok': True,
            'result': {
                'id': 11111,
                'is_bot': True,
                'first_name': 'Name',
                'last_name': 'Name',
                'username': 'bot_username',
            }
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/deleteWebhook')
    def _handler_delete_webhook(request):
        return {
            'ok': True,
            'result': True,
        }

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
                        'text': '/next_birthdays_new',
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

    if expected_buttons is not None:
        expected_reply_markup = {
            'inline_keyboard': [
                [
                    {
                        'text': button.title,
                        'callback_data': button.data,
                        'pay': False,
                    }
                ]
                for button in expected_buttons
            ],
        }
    else:
        expected_reply_markup = None

    request = await handler_send_message.wait_call()
    request_data = request['request'].form

    if expected_reply_markup is not None:
        reply_markup = request_data.pop('reply_markup')
        reply_markup = json.loads(reply_markup)
        assert reply_markup == expected_reply_markup
    else:
        assert 'reply_markup' not in request_data

    assert request_data == {
        'chat_id': sender_chat_id,
        'text': expected_message,
    }


@pytest.mark.parametrize(
    'sender_chat_id, callback_message_exists, callback_button, '
    'expected_message, expected_buttons',
    [
        pytest.param(
            100500,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=1001,
                context_id=_CONTEXT_ID_NEXT_BDS,
                button_id=_BUTTON_ID_EDIT_BD,
            ),
            'Select option',
            [
                Button(
                    title='Delete',
                    birthday_id=1001,
                    context_id=_CONTEXT_ID_EDIT_BD,
                    button_id=_BUTTON_ID_DELETE_BD,
                ),
                Button(
                    title='Cancel',
                    birthday_id=1001,
                    context_id=_CONTEXT_ID_EDIT_BD,
                    button_id=_BUTTON_ID_CANCEL,
                ),
            ],
            id='ok',
        ),
        pytest.param(
            100501,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=1001,
                context_id=_CONTEXT_ID_NEXT_BDS,
                button_id=_BUTTON_ID_EDIT_BD,
            ),
            'Unauthorized',
            None,
            id='unauthorized',
        ),
        pytest.param(
            100500,
            False,
            Button(
                title='person2 on 20.03',
                birthday_id=1001,
                context_id=_CONTEXT_ID_NEXT_BDS,
                button_id=_BUTTON_ID_EDIT_BD,
            ),
            None,
            None,
            id='no_original_message',
        ),
        pytest.param(
            100500,
            True,
            None,
            None,
            None,
            id='no_callback_data',
        ),
        pytest.param(
            100500,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=None,
                context_id=_CONTEXT_ID_NEXT_BDS,
                button_id=_BUTTON_ID_EDIT_BD,
            ),
            'Canceled',
            None,
            id='no_bd_id',
        ),
    ]
)
@pytest.mark.now(_NOW.isoformat())
async def test_edit_birthday(
    service_client,
    pgsql,
    mockserver,
    sender_chat_id: int,
    callback_message_exists: bool,
    callback_button: Optional[Button],
    expected_message: Optional[str],
    expected_buttons: Optional[List[Button]],
):
    # to update mocked time
    await service_client.invalidate_caches()

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getMe')
    def _handler_get_me(request):
        return {
            'ok': True,
            'result': {
                'id': 11111,
                'is_bot': True,
                'first_name': 'Name',
                'last_name': 'Name',
                'username': 'bot_username',
            }
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/deleteWebhook')
    def _handler_delete_webhook(request):
        return {
            'ok': True,
            'result': True,
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getUpdates')
    def _handler_get_updates(request):
        result = {
            'id': 1,
            'from': {
                'id': 22222,
                'is_bot': False,
                'first_name': 'Name'
            },
            'chat_instance': 'unknown',
        }
        if callback_button:
            result['data'] = callback_button.data
        if callback_message_exists:
            result['message'] = {
                'message_id': 2,
                'date': 1,
                'chat': {
                    'id': sender_chat_id,
                    'type': 'private',
                },
                'text': 'Text',
            }
        return {
            'ok': True,
            'result': [
                {
                    'update_id': 1,
                    'callback_query': result,
                }
            ],
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/answerCallbackQuery')
    def handler_answer_callback(request):
        return {
            'ok': True,
            'description': 'foo',
            'result': {
                'callback_query_id': 'query_id',
            }
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/editMessageText')
    def handler_edit_message(request):
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

    if expected_buttons is not None:
        expected_reply_markup = {
            'inline_keyboard': [
                [
                    {
                        'text': button.title,
                        'callback_data': button.data,
                        'pay': False,
                    }
                ]
                for button in expected_buttons
            ],
        }
    else:
        expected_reply_markup = None

    await handler_answer_callback.wait_call()

    if expected_message:
        request = await handler_edit_message.wait_call()
        request_data = request['request'].form

        if expected_reply_markup is not None:
            reply_markup = request_data.pop('reply_markup')
            reply_markup = json.loads(reply_markup)
            assert reply_markup == expected_reply_markup
        else:
            assert 'reply_markup' not in request_data

        assert request_data == {
            'chat_id': sender_chat_id,
            'message_id': 2,
            'text': expected_message,
        }
    else:
        assert not handler_edit_message.has_calls


@pytest.mark.parametrize(
    'sender_chat_id, callback_message_exists, callback_button, '
    'expected_message, expect_deletion',
    [
        pytest.param(
            100500,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=1002,
                context_id=_CONTEXT_ID_EDIT_BD,
                button_id=_BUTTON_ID_DELETE_BD,
            ),
            'Deleted',
            True,
            id='delete',
        ),
        pytest.param(
            100501,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=1002,
                context_id=_CONTEXT_ID_EDIT_BD,
                button_id=_BUTTON_ID_DELETE_BD,
            ),
            'Unauthorized',
            False,
            id='unauthorized',
        ),
        pytest.param(
            100500,
            False,
            Button(
                title='person2 on 20.03',
                birthday_id=1002,
                context_id=_CONTEXT_ID_EDIT_BD,
                button_id=_BUTTON_ID_DELETE_BD,
            ),
            None,
            False,
            id='no_original_message',
        ),
        pytest.param(
            100500,
            True,
            None,
            None,
            False,
            id='no_callback_data',
        ),
        pytest.param(
            100500,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=None,
                context_id=_CONTEXT_ID_EDIT_BD,
                button_id=_BUTTON_ID_DELETE_BD,
            ),
            'Canceled',
            False,
            id='no_bd_id',
        ),
    ]
)
@pytest.mark.now(_NOW.isoformat())
async def test_delete_birthday(
    service_client,
    pgsql,
    mockserver,
    sender_chat_id: int,
    callback_message_exists: bool,
    callback_button: Optional[Button],
    expected_message: str,
    expect_deletion: Optional[List[Button]],
):
    initial_birthdays = [
        dict(month=1, day=16, id=1000),
        dict(month=1, day=17, id=1001),
        dict(month=3, day=20, id=1002),
        dict(month=4, day=17, id=1003),
        dict(month=12, day=20, id=1004),
    ]

    initial_birthdays_in_db = [
        {
            'person': f'person{i}',
            'year': None,
            'month': event['month'],
            'day': event['day'],
            'is_enabled': True,
            'last_notification_time': _NOW,
            'id': event['id'],
        }
        for i, event in enumerate(initial_birthdays)
    ]

    for row in initial_birthdays_in_db:
        insert_birthday(
            pgsql,
            person=row['person'],
            month=row['month'],
            day=row['day'],
            is_enabled=row['is_enabled'],
            id=row['id'],
            last_notification_time=row['last_notification_time'],
            year=row['year'],
        )

    # to update mocked time
    await service_client.invalidate_caches()

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getMe')
    def _handler_get_me(request):
        return {
            'ok': True,
            'result': {
                'id': 11111,
                'is_bot': True,
                'first_name': 'Name',
                'last_name': 'Name',
                'username': 'bot_username',
            }
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/deleteWebhook')
    def _handler_delete_webhook(request):
        return {
            'ok': True,
            'result': True,
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getUpdates')
    def _handler_get_updates(request):
        result = {
            'id': 1,
            'from': {
                'id': 22222,
                'is_bot': False,
                'first_name': 'Name'
            },
            'chat_instance': 'unknown',
        }
        if callback_button:
            result['data'] = callback_button.data
        if callback_message_exists:
            result['message'] = {
                'message_id': 2,
                'date': 1,
                'chat': {
                    'id': sender_chat_id,
                    'type': 'private',
                },
                'text': 'Text',
            }
        return {
            'ok': True,
            'result': [
                {
                    'update_id': 1,
                    'callback_query': result,
                }
            ],
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/answerCallbackQuery')
    def handler_answer_callback(request):
        return {
            'ok': True,
            'description': 'foo',
            'result': {
                'callback_query_id': 'query_id',
            }
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/editMessageText')
    def handler_edit_message(request):
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

    await handler_answer_callback.wait_call()

    if expected_message:
        request = await handler_edit_message.wait_call()
        request_data = request['request'].form
        assert 'reply_markup' not in request_data

        assert request_data == {
            'chat_id': sender_chat_id,
            'message_id': 2,
            'text': expected_message,
        }
    else:
        assert not handler_edit_message.has_calls

    bd_birthdays = fetch_birthdays(pgsql)
    if expect_deletion:
        assert bd_birthdays == (
            initial_birthdays_in_db[:2] + initial_birthdays_in_db[3:]
        )
    else:
        assert bd_birthdays == initial_birthdays_in_db


@pytest.mark.parametrize(
    'sender_chat_id, callback_message_exists, callback_button, '
    'expected_message',
    [
        pytest.param(
            100500,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=1001,
                context_id=_CONTEXT_ID_EDIT_BD,
                button_id=_BUTTON_ID_CANCEL,
            ),
            'Canceled',
            id='cancel_edit',
        ),
        pytest.param(
            100500,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=1001,
                context_id=_CONTEXT_ID_EDIT_BD,
                button_id=_BUTTON_ID_CANCEL,
            ),
            'Canceled',
            id='cancel_delete',
        ),
        pytest.param(
            100501,
            True,
            Button(
                title='person2 on 20.03',
                birthday_id=1001,
                context_id=_CONTEXT_ID_EDIT_BD,
                button_id=_BUTTON_ID_CANCEL,
            ),
            'Unauthorized',
            id='unauthorized',
        ),
        pytest.param(
            100500,
            False,
            Button(
                title='person2 on 20.03',
                birthday_id=1001,
                context_id=_CONTEXT_ID_EDIT_BD,
                button_id=_BUTTON_ID_CANCEL,
            ),
            None,
            id='no_original_message',
        ),
        pytest.param(
            100500,
            True,
            None,
            None,
            id='no_callback_data',
        ),
    ]
)
@pytest.mark.now(_NOW.isoformat())
async def test_cancel(
    service_client,
    pgsql,
    mockserver,
    sender_chat_id: int,
    callback_message_exists: bool,
    callback_button: Optional[Button],
    expected_message: Optional[str],
):
    # to update mocked time
    await service_client.invalidate_caches()

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getMe')
    def _handler_get_me(request):
        return {
            'ok': True,
            'result': {
                'id': 11111,
                'is_bot': True,
                'first_name': 'Name',
                'last_name': 'Name',
                'username': 'bot_username',
            }
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/deleteWebhook')
    def _handler_delete_webhook(request):
        return {
            'ok': True,
            'result': True,
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/getUpdates')
    def _handler_get_updates(request):
        result = {
            'id': 1,
            'from': {
                'id': 22222,
                'is_bot': False,
                'first_name': 'Name'
            },
            'chat_instance': 'unknown',
        }
        if callback_button:
            result['data'] = callback_button.data
        if callback_message_exists:
            result['message'] = {
                'message_id': 2,
                'date': 1,
                'chat': {
                    'id': sender_chat_id,
                    'type': 'private',
                },
                'text': 'Text',
            }
        return {
            'ok': True,
            'result': [
                {
                    'update_id': 1,
                    'callback_query': result,
                }
            ],
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/answerCallbackQuery')
    def handler_answer_callback(request):
        return {
            'ok': True,
            'description': 'foo',
            'result': {
                'callback_query_id': 'query_id',
            }
        }

    @mockserver.json_handler(f'/bot{_TELEGRAM_TOKEN}/editMessageText')
    def handler_edit_message(request):
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

    await handler_answer_callback.wait_call()

    if expected_message:
        request = await handler_edit_message.wait_call()
        request_data = request['request'].form
        assert 'reply_markup' not in request_data

        assert request_data == {
            'chat_id': sender_chat_id,
            'message_id': 2,
            'text': expected_message,
        }
    else:
        assert not handler_edit_message.has_calls
