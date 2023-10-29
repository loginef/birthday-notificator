import datetime as dt

import pytest
import pytz

from testsuite.databases import pgsql

_TZ_MOSCOW = pytz.timezone('Europe/Moscow')
_NOW = _TZ_MOSCOW.localize(dt.datetime(2023, 3, 15, 12))

_TELEGRAM_TOKEN = 'fake_token'


@pytest.mark.now(_NOW.isoformat())
async def test_start(service_client, mockserver):
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
                            'id': 100500,
                            'type': 'private',
                        },
                        'text': '/start',
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
                'message_id': 2,
                'date': 1,
                'chat': {
                    'id': 100500,
                    'type': 'private',
                },
                'text': 'Hi',
            },
        }

    request = await handler_send_message.wait_call()
    assert request['request'].form == {
        'chat_id': 100500,
        'text': 'Hi!',
    }


@pytest.mark.now(_NOW.isoformat())
async def test_chat_id(service_client, mockserver):
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
                            'id': 100500,
                            'type': 'private',
                        },
                        'text': '/chat_id',
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
                    'id': 100500,
                    'type': 'private',
                },
                'text': 'Text',
            },
        }

    request = await handler_send_message.wait_call()
    assert request['request'].form == {
        'chat_id': 100500,
        'text': 'Your chat id is 100500',
    }
