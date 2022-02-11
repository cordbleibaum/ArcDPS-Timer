import datetime
import os

from fastapi import FastAPI
from pydantic import BaseModel
from motor.motor_asyncio import AsyncIOMotorClient


db_client: AsyncIOMotorClient = None

async def get_db():
    return db_client.arcdps_timer


async def connect_db():
    global db_client
    db_client = AsyncIOMotorClient(os.environ['MONGODB'])


async def close_db():
    db_client.close()


class TimingInfoModel(BaseModel):
    time: datetime.datetime
    update_time: datetime.datetime


class ActionInfoModel(BaseModel):
    update_time: datetime.datetime


app = FastAPI()
app.add_event_handler("startup", connect_db)
app.add_event_handler("shutdown", close_db)

@app.get("/")
async def root():
    return {"server_status": "working"}


@app.get("/version")
async def get_version():
    return {
        "major": 6
    }


@app.get("/groups/{group_id}")
async def read_group(group_id):
    db = await get_db()
    group = await db.groups.find_one({'_id': group_id})

    if group is not None:
        return group

    return {"_id": group_id}


@app.post("/groups/{group_id}/start")
async def start_timer(group_id, start: TimingInfoModel):
    db = await get_db()
    group = await db.groups.find_one({'_id': group_id})
    if group and group['status'] == 'running':
        is_newer = group['start_time'] < start.time
        if is_newer or group['status'] != 'running':
            group['start_time'] = start.time
            group['status'] = 'running'
            group['update_time'] = start.update_time
            await db.groups.replace_one({'_id': group_id}, group)
    else:
        await db.groups.update_one(
            {'_id': group_id},
            {
                '$set': {
                    '_id': group_id,
                    'status': 'running',
                    'start_time': start.time,
                    'update_time': start.update_time
                }
            }, True)
    return {'status': 'success'}


@app.post("/groups/{group_id}/stop")
async def stop_timer(group_id, stop: TimingInfoModel):
    db = await get_db()
    group = await db.groups.find_one({'_id': group_id})
    if group and group['status'] == 'running':
        group['status'] = 'stopped'
        group['stop_time'] = stop.time
        group['update_time'] = stop.update_time
        await db.groups.replace_one({'_id': group_id}, group)
    elif group and group['status'] == 'stopped':
        is_older = group['stop_time'] > stop.time
        if is_older:
            group['stop_time'] = stop.time
            group['update_time'] = stop.update_time
            await db.groups.replace_one({'_id': group_id}, group)
    elif group and group['status'] == 'prepared':
        group['status'] = 'stopped'
        group['stop_time'] = stop.time
        group['update_time'] = stop.update_time
        await db.groups.replace_one({'_id': group_id}, group)
    return {'status': 'success'}


@app.get("/groups/{group_id}/reset")
async def reset_timer(group_id, reset: ActionInfoModel):
    db = await get_db()
    await db.groups.update_one({'_id': group_id}, {
        '$set': {
            '_id': group_id,
            'status': 'resetted',
            'update_time': reset.update_time
        }
    });
    return {'status': 'success'}


@app.get("/groups/{group_id}/prepare")
async def prepare_timer(group_id, prepare: ActionInfoModel):
    db = await get_db()
    await db.groups.update_one({'_id': group_id}, {
        '$set': {
            '_id': group_id,
            'status': 'prepared',
             'update_time': prepare.update_time
        }
    }, True);
    return {'status': 'success'}
