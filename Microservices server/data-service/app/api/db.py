import os

from sqlalchemy import (Column, DateTime, Integer, Float, MetaData, String, Table,
                        create_engine, ARRAY)

from databases import Database

DATABASE_URI = os.getenv('DATABASE_URI')

engine = create_engine(DATABASE_URI)
metadata = MetaData()

data = Table(
    'data',
    metadata,
    Column('id', Integer, primary_key=True, autoincrement=True),
    Column('Temperature', Float),
    Column('Pressure', Float),
    Column('Gas', Float),
    Column('H2', Float),
    Column('Humidity', Float)
)

database = Database(DATABASE_URI)