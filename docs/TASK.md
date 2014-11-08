
# Task

## Task state

### State diagram
    +-------+     +---------+
    | Ready | --> | Running |
    +-------+     +---------+
        ^              |
        |              v
        |         +---------+
        +---------| Blocked |
		  +---------+

## Type of tasks
- Normal (tickfull) task
- Tickless task
- Special task
  - System task (incl. scheduler)
  - Idle task per processor core

