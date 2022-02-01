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


class TimingStopModel(BaseModel):
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
    group = await db.groups.find_one({'_id': group_id})

    if group is not None:
        return group

    return {"_id": group_id}


@app.post("/groups/{group_id}/start")
async def start_timer(group_id, start: TimingStartModel):
    db = await get_db()
    group = await db.groups.find_one({'_id': group_id})
    if group:
        is_newer = (group['start_time'] - datetime.timedelta(seconds=group['delta'])) < (start.time - datetime.timedelta(seconds=start.delta))
        if is_newer or group['status'] != 'running':
            group['start_time'] = start.time
            group['delta'] = start.delta
            group['status'] = 'running'
            await db.groups.replace_one({'_id': group_id}, group)
    else:
        await db.groups.insert_one({
            '_id': group_id,
            'status': 'running',
            'start_time': start.time,
            'delta': start.delta
        })
    return {'status': 'success'}


@app.post("/groups/{group_id}/stop")
async def stop_timer(group_id, stop: TimingStopModel):
    db = await get_db()
    group = await db.groups.find_one({'_id': group_id})
    if group and group.status == 'running':
        group['status'] = 'stopped'
        group['stop_time'] = stop.time
        await db.groups.replace_one({'_id': group_id}, group)
    elif group and group.status == 'running':
        is_older = group['stop_time'] > stop.time
        if is_older:
            group['stop_time'] = stop.time
            await db.groups.replace_one({'_id': group_id}, group)
    return {'status': 'success'}


@app.get("/groups/{group_id}/reset")
async def reset_timer(group_id):
    db = await get_db()
    group = await db.groups.find_one({'_id': group_id})
    if group:
        group['status'] = 'resetted'
        await db.groups.replace_one({'_id': group_id}, group)
    return {'status': 'success'}
