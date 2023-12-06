from models import DataIn
from db import data, database


async def add_data(payload: DataIn):
    query = data.insert().values(**payload)
    return await database.execute(query=query)

async def get_all_data():
    query = data.select()
    return await database.fetch_all(query=query)

async def get_data(id):
    query = data.select(data.c.id==id)
    return await database.fetch_one(query=query)

async def delete_data(id: int):
    query = data.delete().where(data.c.id==id)
    return await database.execute(query=query)

async def update_data(id: int, payload: DataIn):
    query = (
        data
        .update()
        .where(data.c.id == id)
        .values(**payload.dict())
    )
    return await database.execute(query=query)