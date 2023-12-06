from typing import List
from fastapi import APIRouter, HTTPException

from app.api.models import DataOut, DataIn, DataUpdate
from app.api import db_manager
from app.api.service import is_cast_present

data = APIRouter()

@data.post('/', response_model=DataOut, status_code=201)
async def create_data(payload: DataIn):
    data_id = await db_manager.add_data(payload)
    response = {
        'id': data_id,
        **payload.dict()
    }

    return response

@data.get('/', response_model=List[DataOut])
async def get_data():
    return await db_manager.get_all_data()

@data.get('/{id}/', response_model=DataOut)
async def get_data(id: int):
    data = await db_manager.get_data(id)
    if not data:
        raise HTTPException(status_code=404, detail="Data not found")
    return data

@data.put('/{id}/', response_model=DataOut)
async def update_data(id: int, payload: DataUpdate):
    data = await db_manager.get_data(id)
    if not data:
        raise HTTPException(status_code=404, detail="Data not found")

    update_datas = payload.dict(exclude_unset=True)

    if 'casts_id' in update_datas:
        for cast_id in payload.casts_id:
            if not is_cast_present(cast_id):
                raise HTTPException(status_code=404, detail=f"Cast with given id:{cast_id} not found")

    data_in_db = DataIn(**data)

    updated_data = data_in_db.copy(update=update_datas)

    return await db_manager.update_data(id, updated_data)

@data.delete('/{id}/', response_model=None)
async def delete_data(id: int):
    data = await db_manager.get_data(id)
    if not data:
        raise HTTPException(status_code=404, detail="Data not found")
    return await db_manager.delete_data(id)
