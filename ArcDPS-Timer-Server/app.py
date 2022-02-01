import datetime

from fastapi import FastAPI
from pydantic import BaseModel
from motor.motor_asyncio import AsyncIOMotorClient

db_client: AsyncIOMotorClient = None

async def get_db():
    return db_client.arcdps_timer


async def connect_db():
    global db_client
    db_client = AsyncIOMotorClient("mongodb://localhost:27017/")


async def close_db():
    db_client.close()


class TimingStartModel(BaseModel):
    delta: float
    time: datetime.datetime


app = FastAPI()
app.add_event_handler("startup", connect_db)
app.add_event_handler("shutdown", close_db)

@app.get("/")
async def root():
    return {"version": "0.1"}


@app.get("/groups/{group_id}")
async def read_group(group_id):
    db = await get_db()
    group = await db.test_collection.find_one({'_id': group_id});

    if group is not None:
        return group

    return {"_id": group_id}


@app.post("/groups/{group_id}/start")
async def start_timer(start: TimingStartModel):
    db = await get_db()
    return start


@app.post("/groups/{group_id}/retime")
async def retime_timer(retime: TimingStartModel):
    db = await get_db()
    return retime


@app.post("/groups/{group_id}/stop")
async def stop_timer():
    db = await get_db()
    return {}


@app.post("/groups/{group_id}/reset")
async def reset_timer():
    db = await get_db()
    return {}
