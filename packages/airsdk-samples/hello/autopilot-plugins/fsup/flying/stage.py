import logging

from ..mission import UID
from fsup.genstate import State, guidance_modes


SUFFIX = UID + ".flying"
_MOVE_FORWARD_MODE = SUFFIX + ".move_forward"

class flying(State):
    def enter(self, msg):
        logging.info("STATE : ZZZZZZZZZ")
        pass


@guidance_modes(_MOVE_FORWARD_MODE)
class MoveForward(State):
    def enter(self, msg):
        logging.info("VERIF SSTATE : moveForward enter")
        self.set_guidance_mode(_MOVE_FORWARD_MODE)

MOVE_UP_STATE = {
"name": "move_forward",
"class": MoveForward
}

FLYING_STAGE = {
    "name": "flying",
    "class": flying,
    "initial": "camera",
    "children": [MOVE_UP_STATE],
}