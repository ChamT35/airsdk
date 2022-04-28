import logging
import olympe
import olympe.log
import os
from time import sleep
from olympe.controller import Disconnected
from olympe.messages.common.Common import Reboot
from olympe.messages import mission

DRONE_IP = os.environ.get("DRONE_IP", "10.202.0.1")
HELLO_MISSION_URL = os.environ.get("HELLO_MISSION_URL", "out/hello-pc/images/com.parrot.missions.samples.hello.tar.gz")

olympe.log.update_config({"loggers": {"olympe": {"level": "WARNING"}}})
logger = logging.getLogger("olympe")

def camera_mission():
    drone = olympe.Drone(DRONE_IP)
    with drone.mission.from_path(HELLO_MISSION_URL).open() as hello:
        from olympe.airsdk.messages.parrot.missions.samples.hello.Command import Hold, Say

        logger.warning("CAMERA : Installation de la mission")
        assert drone.connect()
        assert hello.install(allow_overwrite=True)
        assert drone(Reboot() >> Disconnected()).wait()

        logger.warning("CAMERA : Mission installée, Activation de la mission")
        assert drone.connect(retry=5)
        assert hello.wait_ready(5)

        mission_activated = drone(mission.state(state="active"))
        mission_activated = drone(
            mission.load(uid=hello.uid)
            >> mission.activate(uid=hello.uid)
        )
        assert mission_activated.wait(), mission_activated.explain()
        logger.warning("CAMERA : Mission activée, Début de la mission")

        logger.warning("MISSION CAMERA : SAY")
        assert drone(Say()).wait()
        sleep(20)
        logger.warning("MISSION CAMERA : HOLD")
        assert drone(Hold()).wait()


        assert drone.disconnect()
        logger.warning("CAMERA : Fin de la mission, Drone déconnecté")

if __name__ == "__main__":
    camera_mission()