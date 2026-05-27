# Task: Client Login Screen Implementation

## Task List

### 1. Create message ID constants module [completed]
- File: `scripts/client/msg_ids.py`
- Define all message ID constants matching `scripts/common/proto/msg_ids.h`
- Constants: MSG_ID_HEARTBEAT, MSG_ID_HEARTBEAT_RESP, MSG_ID_LOGIN_REQ, MSG_ID_LOGIN_RESP, MSG_ID_QUERY_ROLES_REQ, MSG_ID_QUERY_ROLES_RESP, MSG_ID_CREATE_ROLE_REQ, MSG_ID_CREATE_ROLE_RESP, MSG_ID_ENTER_GAME_REQ, MSG_ID_ENTER_GAME_RESP

### 2. Create login flow manager module [completed]
- File: `scripts/client/login_flow.py`
- Implement LoginFlowManager class
- Handle the complete login flow: connect -> login -> query roles -> create role (if needed) -> enter game
- Implement timeout detection (10 seconds per step)
- Implement error handling with user-friendly messages
- Emit state changes and results via callbacks

### 3. Create login screen UI module [completed]
- File: `scripts/client/login_screen.py`
- Implement LoginScreen class using PyGame
- Display "Farm Demo" title
- Display account ID input field with real-time text display
- Display "Enter Game" button
- Display error messages
- Display "connecting..." loading state
- Handle keyboard input (typing, backspace, enter)
- Handle mouse input (button click)

### 4. Update connection module to support AccountMsg wrapping [completed]
- File: `scripts/client/connection.py`
- Add method to send AccountMsg wrapped messages
- Add method to send PlayerMsg wrapped messages
- Ensure message IDs are correctly passed

### 5. Create main client entry point with login screen integration [completed]
- File: `scripts/client/main.py`
- Initialize PyGame
- Initialize logging
- Run login screen
- Handle login success -> transition to game state
- Handle login failure -> show error, allow retry

### 6. Test login screen functionality [completed]
- Verify UI displays correctly
- Verify input handling (typing, backspace, enter)
- Verify button click
- Verify empty input validation
- Verify error message display
