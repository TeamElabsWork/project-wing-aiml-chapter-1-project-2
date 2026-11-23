# ADR-0001: Control & messaging middleware (OPEN)
Options: ROS 2 | lightweight (TCP/ZeroMQ/MQTT). **Current default:** TCP JSON-lines bridge (no dependencies).
Decide before: flight-stack adapter work (C1) and Plan 2 (H2/H3). Criteria: PX4 integration, team familiarity, Windows+Unreal support, latency.
Because only `DroneInterface` implementations touch middleware, the decision is localised to C1.
Status: open. Decision: _TBD_. Date: _TBD_.
