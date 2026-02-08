import bzapi

flags = {}

class Flag:
    def __init__(self, cname: str, activate: bool = True):
        self.cname = cname
        self.name = {}
        self.description = {}
        self.params = {}
        self.callbacks = {}
        if activate:
            flags[self.cname] = self

    def set_parameter(self, key: str, value: object) -> None:
        bzapi.set_data("flags/"+self.cname, key, self.params.get(key))
        self.params[key] = value
    
    def get_parameter(self, key: str) -> object:
        return bzapi.get_data("flags/"+self.cname, key) or self.params.get(key)
    
    def add_callback(self, event_type: bzapi.event_type, callback) -> None:
        self.callbacks[event_type] = callback
    
    def register(self) -> None:
        bzapi.register_flag(self.cname, self.name, self.description, self.params)
        for event, callback in self.callbacks.items():
            bzapi.register_callback(event, callback)

