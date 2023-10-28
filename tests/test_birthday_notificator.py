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
        last_notification_time=_NOW - dt.timedelta(days=365),
        year=1960,
    )
    insert_birthday(
        pgsql,
        person='person2',
        month=3,
        day=15,
        is_enabled=True,
    )
    insert_birthday(
        pgsql,
        person='person3',
        month=3,
        day=15,
        is_enabled=False,
    )
    insert_birthday(
        pgsql,
        person='person4',
        month=3,
        day=14,
        is_enabled=True,
    )

    @testpoint('birthday-notificator')
    async def worker_finished(data):
        pass

    await service_client.run_task('distlock/birthday-notificator')

    assert worker_finished.has_calls
    request = worker_finished.next_call()['data']
    assert request == {
        'forgotten': ['person4 on 14.03'],
        'celebrate_today': ['person1', 'person2'],
    }

    request = await handler_send_message.wait_call()
    assert request['request'].form == {
        'chat_id': 100500,
        'text': (
            'Today is birthday of person1, person2\n'
            'You forgot about birthdays: \n'
            'person4 on 14.03'
        )
    }

    assert fetch_birthdays(pgsql) == [
        dict(
            person='person1',
            month=3,
            day=15,
            is_enabled=True,
            last_notification_time=_NOW,
            year=1960,
        ),
        dict(
            person='person2',
            month=3,
            day=15,
            is_enabled=True,
            last_notification_time=_NOW,
            year=None,
        ),
        dict(
            person='person3',
            month=3,
            day=15,
            is_enabled=False,
            last_notification_time=None,
            year=None,
        ),
        dict(
            person='person4',
            month=3,
            day=14,
            is_enabled=True,
            last_notification_time=_NOW,
            year=None,
        ),
    ]

    await service_client.run_task('distlock/birthday-notificator')
    assert worker_finished.has_calls
    assert not handler_send_message.has_calls
