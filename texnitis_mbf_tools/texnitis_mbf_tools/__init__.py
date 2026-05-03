"""Python tooling for the texnitis mbf navigation stack.

Modules (added in milestone M6):

- ``waypoint_sender``: drives ``mbf_msgs/action/MoveBase`` from a YAML waypoint list
- ``mbf_action_bridge``: legacy ``SetGoal.srv`` → mbf action bridge for migration
- ``nav_state_publisher``: re-emits old ``nav_state`` JSON from mbf feedback / result
"""
