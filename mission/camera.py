import logging
import olympe
import olympe.log
import os
import sys
from time import sleep
from olympe.controller import Disconnected
from olympe.messages.common.Common import Reboot
from olympe.messages import mission
from olympe.messages.camera import take_photo, set_camera_mode, set_photo_mode
from olympe.messages.ardrone3.Piloting import Landing

DRONE_IP = os.environ.get("DRONE_IP", "10.202.0.1")
HELLO_MISSION_URL = os.environ.get("HELLO_MISSION_URL", "out/hello-pc/images/com.parrot.missions.samples.hello.tar.gz")

olympe.log.update_config({"loggers": {"olympe": {"level": "WARNING"}}})
logger = logging.getLogger("olympe")


W  = "\x1b[97m"  # white (normal)
R  = "\x1b[31m" # red
G  = "\x1b[92m" # green
O  = '3[33m' # orange
B  = '3[34m' # blue
P  = '3[35m' # purple


class FlightListener(olympe.EventListener):
    def __init__(self,  *args, **kwds):
        super().__init__(*args, **kwds)
        self.drone = args[0]
        self.photo_count = 0

    @olympe.listen_event()
    def onAnyEvent(self, event, scheduler):
        if event.message.fullName == "parrot.missions.samples.hello.Event.ask_for_photo":
            print(G+"JOB DONE"+W)
            self.photo_count +=1
            print(self.photo_count)
            assert self.drone(take_photo(cam_id=0)).wait()
            assert self.drone(Landing()).wait().success()

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
        try :
            drone(set_camera_mode(cam_id=0, value="photo")).wait()
            drone( take_photo(cam_id=0)).wait()
        except Exception as e:
            print(traceback.format_exc(), file=sys.stderr)
            print("\n")

        with FlightListener(drone) as flight_listener:
            logger.warning("MISSION CAMERA : SAY")
            assert drone(Say()).wait()
            sleep(10)
            
            while(flight_listener.photo_count<10):
                pass
            logger.warning("MISSION CAMERA : HOLD")
            assert drone(Hold()).wait()


        assert drone.disconnect()
        logger.warning("CAMERA : Fin de la mission, Drone déconnecté")

if __name__ == "__main__":
    camera_mission()