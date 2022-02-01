import datetime
from fastapi import FastAPI
from pydantic import BaseModel

app = FastAPI()


class TimingStart(BaseModel):
    delta: float
    time: datetime.datetime


@app.get("/")
async def root():
    return {"version": "0.1"}


@app.get("/groups/{group_id}")
async def read_group(group_id):
    return {"group_id": group_id}


@app.post("/groups/{group_id}/start")
async def start_timer(start: TimingStart):
    return start


@app.post("/groups/{group_id}/retime")
async def retime_timer(retime: TimingStart):
    return retime


@app.post("/groups/{group_id}/stop")
async def stop_timer():
    return {}


@app.post("/groups/{group_id}/reset")
async def reset_timer():
    return {}
