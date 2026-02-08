"""Type stubs for bz_plugins module"""

from enum import IntEnum
from typing import Callable

class EventType(IntEnum):
    PLAYER_JOIN = 1
    PLAYER_LEAVE = 2
    PLAYER_SPAWN = 3
    PLAYER_DIE = 4
    CREATE_SHOT = 5
    CHAT = 6

def register_callback(type: EventType, callback: Callable) -> None:
    """Register a callback for a specific event type"""
    ...

def send_chat_message(from_id: int, to_id: int, text: str) -> None:
    """Send a chat message"""
    ...

def set_player_parameter(client_id: int, parameter: str, value: float) -> None:
    """Set a player parameter"""
    ...

def kill_player(target_id: int) -> None:
    """Kill a player"""
    ...

def disconnect_player(target_id: int, reason: str) -> None:
    """Disconnect a player from the server"""
    ...

def get_player_by_name(name: str) -> int:
    """Get a player ID by name"""
    ...

def get_all_player_ids() -> list[int]:
    """Get all player IDs"""
    ...

def get_player_name(id: int) -> str:
    """Get a player's name by ID"""
    ...

def get_player_ip(id: int) -> str:
    """Get a player's IP by ID"""
    ...
