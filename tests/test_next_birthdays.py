import datetime as dt
from typing import Optional

import pytest
import pytz

from testsuite.databases import pgsql

_TZ_MOSCOW = pytz.timezone('Europe/Moscow')
_NOW = _TZ_MOSCOW.localize(dt.datetime(2023, 3, 15, 12))

#TODO rename test


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

_MAX_RETURNED_ITEMS = 10

@pytest.mark.parametrize(
    'birthdays_in_db, expected_message',
    [
        pytest.param(
            [
                dict(month=1, day=16),
                dict(month=1, day=17),
                dict(month=2, day=17),
                dict(month=3, day=20),
                dict(month=12, day=20),
            ],
            '',
            id='basic',
        ),
        pytest.param(
            [
                dict(month=1, day=16),
                dict(month=1, day=17),
                dict(month=2, day=17),
                dict(month=3, day=i) for i in range(1,10),
                dict(month=3, day=20),
                dict(month=12, day=20),
            ],
            '',
            id='too_many_events',
        ),
        pytest.param(
            [
                dict(month=1, day=15),
                dict(month=1, day=17),
                dict(month=2, day=17),
                dict(month=3, day=i) for i in range(1,10),
                dict(month=3, day=20),
                dict(month=12, day=20),
            ],
            '',
            id='include_today',
        ),
    ]
)
@pytest.mark.now(_NOW.isoformat())
async def test_next_birthdays(
    service_client, 
    pgsql, 
    testpoint,
    birthdays_in_db: List[Dict[str, Any]],
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

    @testpoint('bot-send-message')
    async def send_message(data):
        pass

    await service_client.run_task('list-birthdays')

    # assert send_message.has_calls
    request = await send_message.wait_call()
    assert request['data'] == {
        'text': (
            'Today is birthday of person1, person2\n'
            'You forgot about birthdays: \n'
            'person4 on 14.03'
        )
    }
