from pydantic import BaseModel
from typing import List, Optional

class DataIn(BaseModel):
    Temperature: float
    Pressure: float
    Gas: float
    H2: float
    Humidity: float

class DataOut(DataIn):
    id: int


class DataUpdate(DataIn):
    Temperature: Optional[float] = None
    Pressure: Optional[float] = None
    Gas: Optional[float] = None
    H2: Optional[float] = None
    Humidity: Optional[float] = None