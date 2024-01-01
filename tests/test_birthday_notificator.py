import datetime as dt
from typing import Optional

import pytest
import pytz

from testsuite.databases import pgsql

_TZ_MOSCOW = pytz.timezone('Europe/Moscow')
_NOW = _TZ_MOSCOW.localize(dt.datetime(2023, 3, 15, 12))

_TELEGRAM_TOKEN = 'fake_token'


def insert_birthday(
    pgsql,
    person: str,
    month: int,
    day: int,
    is_enabled: bool,
    user_id: int,
    last_notification_time: Optional[dt.datetime] = None,
    year: Optional[int] = None,
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
            user_id
        )
        VALUES (%s, %s, %s, %s, %s, %s, %s)
        """,
        (person, year, month, day, is_enabled, last_notification_time, user_id)
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
        VALUES (1000, 100500), (1001, 100501), (1002, 100502)
        """,
    ],
)
@pytest.mark.now(_NOW.isoformat())
async def test_notification(service_client, pgsql, testpoint, mockserver):
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

    insert_birthday(
        pgsql,
        person='person1',
        month=3,
        day=15,
        is_enabled=True,
        user_id=1000,
        last_notification_time=_NOW - dt.timedelta(days=365),
        year=1960,
    )
    insert_birthday(
        pgsql,
        person='person2',
        month=3,
        day=15,
        is_enabled=True,
        user_id=1000,
    )
    insert_birthday(
        pgsql,
        person='person3',
        month=3,
        day=15,
        is_enabled=False,
        user_id=1000,
    )
    insert_birthday(
        pgsql,
        person='person4',
        month=3,
        day=14,
        is_enabled=True,
        user_id=1000,
    )
    insert_birthday(
        pgsql,
        person='person5',
        month=3,
        day=15,
        is_enabled=True,
        user_id=1002,
    )
    insert_birthday(
        pgsql,
        person='person6',
        month=3,
        day=14,
        is_enabled=True,
        user_id=1002,
    )

    @testpoint('birthday-notificator')
    async def worker_finished(data):
        pass

    await service_client.run_task('distlock/birthday-notificator')

    assert worker_finished.has_calls
    request = worker_finished.next_call()['data']
    assert request == {
        '1000': {
            'forgotten': ['person4 on 14.03'],
            'celebrate_today': ['person1', 'person2'],
        },
        '1002': {
            'forgotten': ['person6 on 14.03'],
            'celebrate_today': ['person5'],
        },
    }

    assert handler_send_message.times_called == 2
    requests = dict()
    for i in range(2):
        request = await handler_send_message.wait_call()
        requests[request['request'].form['chat_id']] = request['request'].form

    assert requests == {
        100500: {
            'chat_id': 100500,
            'text': (
                'Today is birthday of person1, person2\n'
                'You forgot about birthdays: \n'
                'person4 on 14.03'
            )
        },
        100502: {
            'chat_id': 100502,
            'text': (
                'Today is birthday of person5\n'
                'You forgot about birthdays: \n'
                'person6 on 14.03'
            )
        },
    }

    assert fetch_birthdays(pgsql) == [
        dict(
            person='person1',
            month=3,
            day=15,
            is_enabled=True,
            last_notification_time=_NOW,
            year=1960,
            user_id=1000,
        ),
        dict(
            person='person2',
            month=3,
            day=15,
            is_enabled=True,
            last_notification_time=_NOW,
            year=None,
            user_id=1000,
        ),
        dict(
            person='person3',
            month=3,
            day=15,
            is_enabled=False,
            last_notification_time=None,
            year=None,
            user_id=1000,
        ),
        dict(
            person='person4',
            month=3,
            day=14,
            is_enabled=True,
            last_notification_time=_NOW,
            year=None,
            user_id=1000,
        ),
        dict(
            person='person5',
            month=3,
            day=15,
            is_enabled=True,
            last_notification_time=_NOW,
            year=None,
            user_id=1002,
        ),
        dict(
            person='person6',
            month=3,
            day=14,
            is_enabled=True,
            last_notification_time=_NOW,
            year=None,
            user_id=1002,
        ),
    ]

    await service_client.run_task('distlock/birthday-notificator')
    assert worker_finished.has_calls
    assert not handler_send_message.has_calls
