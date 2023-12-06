from fastapi_mqtt import FastMQTT, MQTTConfig
import asyncio
import json
import logging
import asyncio
import sys
import time
import tensorflow as tf
import ssl
import smtplib
import pandas as pd
from werkzeug.exceptions import abort
from email.message import EmailMessage
import numpy as np
from tensorflow.keras.models import load_model
from datetime import datetime
from typing import Iterator
from fastapi import FastAPI
from fastapi.requests import Request
from fastapi.responses import HTMLResponse, StreamingResponse
from fastapi.templating import Jinja2Templates
from starlette.responses import Response
from typing import List
from app.api.models import DataOut, DataIn, DataUpdate
from app.api.db import metadata, database, engine
logging.basicConfig(stream=sys.stdout, level=logging.INFO, format="%(asctime)s %(levelname)s %(message)s")
logger = logging.getLogger(__name__)

temp_value = 2
humid_value = 2
gas_value = 2
pressure_value = 2
hidro_value = 2
feature_dict = {}
values = []
keys = []

pred = None

sample_df = pd.DataFrame(columns=['Temperature[C]', 'Humidity[%]', 'Raw H2', 'Raw Ethanol', 'Pressure[hPa]'])
temp_min_max = [22, 80]
humid_min_max = [10.74, 75.2]
rawH2_min_max = [0.2, 5.0]
rawEthanol_min_max = [140.0, 1024.96]
pressure_min_max = [930.852, 939.861]

application = FastAPI(openapi_url="/api/v1/casts/openapi.json", docs_url="/api/v1/casts/docs")
templates = Jinja2Templates(directory="app/api/templates")


@application.on_event("startup")
async def startup():
    asyncio.create_task(predict_fire()) 

mqtt_config = MQTTConfig(host = "********",
    port= 1883,
    keepalive = 60,
    username="hunglp",
    password="phihung21522115")

mqtt = FastMQTT(
    config=mqtt_config
)

mqtt.init_app(application)


@mqtt.on_connect()
def connect(client, flags, rc, properties):
    mqtt.client.subscribe("/mqtt") #subscribing mqtt topic
    print("Connected: ", client, flags, rc, properties)


@mqtt.on_message()
async def message(client, topic, payload, qos, properties):
    print("Received message received from the topic", topic, ": ", payload.decode())

@mqtt.subscribe("esp/bme680/temperature")
async def message_to_topic(client, topic, payload, qos, properties):
    global temp_value
    
    temp_value = float(payload.decode())
    print("Temperature value received from the topic", topic, ": ", payload.decode())

@mqtt.subscribe("esp/bme680/humidity")
async def message_to_topic(client, topic, payload, qos, properties):
    print("Humidity value received from the topic", topic, ": ", payload.decode())
    global humid_value
    humid_value = float(payload.decode())


@mqtt.subscribe("esp/bme680/pressure")
async def message_to_topic(client, topic, payload, qos, properties):
    print("Pressure value received from the topic", topic, ": ", payload.decode())
    global pressure_value
    pressure_value = float(payload.decode())

@mqtt.subscribe("esp/bme680/gas")
async def message_to_topic(client, topic, payload, qos, properties):
    print("Gas value received from the topic", topic, ": ", payload.decode())
    global gas_value
    gas_value = float(payload.decode())

@mqtt.subscribe("esp/bme680/hidro")
async def message_to_topic(client, topic, payload, qos, properties):
    print("Hidro value received from the topic", topic, ": ", payload.decode())
    global hidro_value
    hidro_value = float(payload.decode())

@application.get("/api/v1/casts/temperature", response_class=HTMLResponse)
async def index(request: Request) -> Response:
    return templates.TemplateResponse("index.html", {"request": request})

@application.get("/api/v1/casts/gas", response_class=HTMLResponse)
async def get_gas(request: Request) -> Response:
    return templates.TemplateResponse("gas.html", {"request": request})

@application.get("/api/v1/casts/pressure", response_class=HTMLResponse)
async def get_pressure(request: Request) -> Response:
    return templates.TemplateResponse("pressure.html", {"request": request})

@application.get("/api/v1/casts/humidity", response_class=HTMLResponse)
async def get_humidity(request: Request) -> Response:
    return templates.TemplateResponse("humidity.html", {"request": request})

@application.get("/api/v1/casts/hidro", response_class=HTMLResponse)
async def get_hidro(request: Request) -> Response:
    return templates.TemplateResponse("hidro.html", {"request": request})

async def send_email(temp, humid, hidro, gas, pressure):
    email_sender = 'phihung21522115@gmail.com'
    email_password = '*********'
    email_receiver = '21522115@gm.uit.edu.vn'
    subject = "Về việc xảy ra cháy"
    body = f"""
    #Hiện đang có cháy với nhiệt độ hiện tại là {temp},
    #độ ẩm hiện tại là {humid},
    ##lượng khí hydrogen hiện tại là {hidro},
    #lượng khí ethanol hiện tại là {gas},
    #áp suất hiện tại là {pressure}.
    """
    em = EmailMessage()
    em ['From'] = email_sender
    em ['To'] = email_receiver
    em ['subject'] = subject
    em.set_content (body)
    context = ssl.create_default_context()
    with smtplib.SMTP_SSL('smtp.gmail.com', 465, context=context) as smtp:
        smtp.login (email_sender, email_password)
        smtp.sendmail(email_sender, email_receiver, em.as_string())
    print("\nThe mail has been sent\n")

def Min_Max_Scaler(x, x_min, x_max):
  scaled_data = (x-x_min)/(x_max-x_min)
  return scaled_data
 
def convert_to_binary(output, threshold=0.5):
    binary_output = 0 if np.max(output) < threshold else 1
    return binary_output

def Scaling_Data():
   scaled_values = []
   scaled_values.append(Min_Max_Scaler(temp_value, temp_min_max[0], temp_min_max[1]))
   scaled_values.append(Min_Max_Scaler(humid_value, humid_min_max[0], humid_min_max[1]))
   scaled_values.append(Min_Max_Scaler(hidro_value, rawH2_min_max[0], rawH2_min_max[1]))
   scaled_values.append(Min_Max_Scaler(gas_value, rawEthanol_min_max[0], rawEthanol_min_max[1]))
   scaled_values.append(Min_Max_Scaler(pressure_value, pressure_min_max[0], pressure_min_max[1]))
   return scaled_values

sample_data = {}
async def predict_fire():
    global temp_value, humid_value, gas_value, pressure_value, hidro_value
    global sample_df
    while True:
        sample_data["Temperature[C]"] = temp_value
        sample_data["Humidity[%]"] = humid_value
        sample_data["Raw H2"] = hidro_value
        sample_data["Raw Ethanol"] = gas_value
        sample_data["Pressure[hPa]"] = pressure_value
        sample_df = pd.DataFrame([sample_data])

        global feature_dict, values, keys
        values = [temp_value, humid_value, hidro_value, gas_value, pressure_value]
        keys = ["Temperature[C]",	"Humidity[%]",	"Raw H2",	"Raw Ethanol",	"Pressure[hPa]"]

        data_scaled = Scaling_Data()
        data_scaled = np.hstack(data_scaled).tolist()
        feature_dict = dict(zip(keys, data_scaled))
        df_feature = pd.DataFrame(feature_dict, index=[0])

        saved_model = tf.keras.models.load_model("app/BiLSTM.h5")
        pred = saved_model.predict(df_feature)
        print(convert_to_binary(pred, threshold=0.5))
        print("\nThe mail has been sent\n")
        await asyncio.sleep(10)

async def generate_random_data(request: Request) -> Iterator[str]:
    client_ip = request.client.host
    logger.info("Client %s connected", client_ip)

    while True:
        json_data = json.dumps(
            {
                "time": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
                "value": temp_value,
            }
        )
        yield f"data:{json_data}\n\n"
        await asyncio.sleep(1)

@application.get("/chart-data")
async def chart_data(request: Request) -> StreamingResponse:
    response = StreamingResponse(generate_random_data(request), media_type="text/event-stream")
    response.headers["Cache-Control"] = "no-cache"
    response.headers["X-Accel-Buffering"] = "no"
    return response