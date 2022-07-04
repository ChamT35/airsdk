import logging
import os
import pprint

import olympe
import olympe.log
from olympe.controller import Disconnected
from olympe.messages import mission
from olympe.messages.common.Common import Reboot

DRONE_IP = os.environ.get("DRONE_IP", "10.202.0.1")
HELLO_MISSION_URL = os.environ.get(
    "HELLO_MISSION_URL", "out/hello-pc/images/com.parrot.missions.samples.hello.tar.gz")
olympe.log.update_config({"loggers": {"olympe": {"level": "WARNING"}}})
logger = logging.getLogger("olympe")


def test_hello_mission():
    logger.warning("Chargement de la mission")
    drone = olympe.Drone(DRONE_IP)
    with drone.mission.from_path(HELLO_MISSION_URL).open() as hello:

        from olympe.airsdk.messages.parrot.missions.samples.hello.Command import Hold, Say
        from olympe.airsdk.messages.parrot.missions.samples.hello.Event import count

        logger.warning("Installation de la mission")

        assert drone.connect()
        assert hello.install(allow_overwrite=True)
        logger.info("Mission list: " +
                    pprint.pformat(drone.mission.list_remote()))
        assert drone(Reboot() >> Disconnected()).wait()

        logger.warning("Mission installée")

        assert drone.connect(retry=5)
        assert hello.wait_ready(5)

        logger.warning("Drone Connecté")

        mission_activated = drone(
            mission.load(uid=hello.uid)
            >> mission.activate(uid=hello.uid)
        )
        logger.warning("Activation de la mission")
        assert mission_activated.wait(), mission_activated.explain()

        logger.warning("Hello mission activé")
        
        assert drone(Say()).wait()
        counter = None

        logger.warning("Mission : Hello")
        # Wait for 3 nod of the drone gimbal

        for i in range(3):
            if counter is None:
                expectation = drone(count(_policy="wait")).wait(_timeout=10)
                assert expectation, expectation.explain()
                counter = expectation.received_events().last().args["value"]
            else:
                counter += 1
                expectation = drone(count(value=counter)).wait(_timeout=10)
                assert expectation, expectation.explain()
        logger.warning("Mission : 3 Hello")
        # Stop and disconnect
        assert drone(Hold()).wait()
        expectation = drone(count(_policy="wait", _timeout=5)).wait()
        assert not expectation, expectation.explain()
        assert drone(count(value=counter, _policy="check"))
        assert drone.disconnect()
        logger.warning("Drone déconnecté")


if __name__ == "__main__":
    test_hello_mission()
