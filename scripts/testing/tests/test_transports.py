from web_ui_test.transports import DeviceRequestThrottle


class Route:
    def __init__(self):
        self.continued = False
        self.request = type("Request", (), {"url": "http://device/api"})()

    def continue_(self):
        self.continued = True


def test_request_throttle_always_continues_route():
    route = Route()
    DeviceRequestThrottle(0)(route)
    assert route.continued
