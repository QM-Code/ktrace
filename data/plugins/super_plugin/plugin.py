import bzapi

def handler(player_id : int, shot_id : int) -> bool:
    name = bzapi.get_player_name(player_id)  # type: ignore
    if name == 'grue':
        return True
    else:
        return False
    
bzapi.register_callback(bzapi.EventType.PLAYER_DIE, handler)