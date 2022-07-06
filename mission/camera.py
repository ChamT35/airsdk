import logging
import os
import pprint
import sys
import traceback

import olympe
import olympe.log
from olympe.controller import Disconnected
from olympe.messages.camera import take_photo, set_camera_mode, set_photo_mode


DRONE_IP = os.environ.get("DRONE_IP", "10.202.0.1")

olympe.log.update_config({"loggers": {"olympe": {"level": "WARNING"}}})
logger = logging.getLogger("olympe")


def takePhoto(drone):
    try:
        drone(set_camera_mode(cam_id=0, value="photo")).wait()
        drone(take_photo(cam_id=0)).wait()
    except Exception as e:
        print(traceback.format_exc(), file=sys.stderr)
        print("\n")


if __name__ == "__main__":
    drone = olympe.Drone(DRONE_IP)
    assert drone.connect(retry=5)
    takePhoto(drone)
    assert drone.disconnect()