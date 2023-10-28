import pathlib

import pytest

from testsuite.databases.pgsql import discover


pytest_plugins = ['pytest_userver.plugins.postgresql']

USERVER_CONFIG_HOOKS = ['userver_config_telegram_bot']

TELEGRAM_TOKEN = 'fake_token'


@pytest.fixture(scope='session')
def service_source_dir():
    """Path to root directory service."""
    return pathlib.Path(__file__).parent.parent


@pytest.fixture(scope='session')
def pgsql_local(service_source_dir, pgsql_local_create):
    """Create schemas databases for tests"""
    databases = discover.find_schemas(
        'telegram_bot',  # service name that goes to the DB connection
        [service_source_dir.joinpath('postgresql/schemas')],
    )
    return pgsql_local_create(list(databases.values()))


@pytest.fixture(scope='session')
def userver_config_telegram_bot(mockserver_info):
    def do_patch(config_yaml, config_vars):
        components = config_yaml['components_manager']['components']
        components['telegram-bot'][
            'telegram_host'
        ] = mockserver_info.base_url.strip('/')

    return do_patch
    # /// [patch configs]


@pytest.fixture(autouse=True)
def delete_webhook_handler(mockserver):
    @mockserver.json_handler(f'/bot{TELEGRAM_TOKEN}/deleteWebhook')
    def _handler_delete_webhook(request):
        return {
            'ok': True,
            'result': True,
        }


@pytest.fixture(autouse=True)
def get_me_handler(mockserver):
    @mockserver.json_handler(f'/bot{TELEGRAM_TOKEN}/getMe')
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


@pytest.fixture(autouse=True)
def get_updates_handler(mockserver):
    @mockserver.json_handler(f'/bot{TELEGRAM_TOKEN}/getUpdates')
    def _handler_get_updates(request):
        return {
            'ok': True,
            'result': [],
        }
