import json, boto3
from boto3.dynamodb.conditions import Key
from decimal import Decimal

# Helper to fix JSON serialization errors
class DecimalEncoder(json.JSONEncoder):
    def default(self, obj):
        if isinstance(obj, Decimal):
            return float(obj)
        return super(DecimalEncoder, self).default(obj)

dynamo = boto3.resource("dynamodb")
table  = dynamo.Table("FootStepData")

def lambda_handler(event, context):
    resp = table.query(
        KeyConditionExpression=Key("device").eq("ESP32-C6 FootStep"),
        ScanIndexForward=False,   # newest first
        Limit=1
    )
    items = resp.get("Items", [])
    if not items:
        return {"statusCode": 200, "body": json.dumps({"steps": 0, "voltage_mv": 0})}

    item = items[0]
    
    # Extracted nested payload
    payload = item.get("payload", {})
    
    # If AWS IoT Core saved the payload as a string instead of a map, parse it
    if isinstance(payload, str):
        try:
            payload = json.loads(payload)
        except:
            pass

    return {
        "statusCode": 200,
        "headers": {
            "Access-Control-Allow-Origin": "*"   # allow the S3 page to call this
        },
        "body": json.dumps({
            "steps":      int(payload.get("steps", 0)),
            "voltage_mv": round(float(payload.get("voltage", 0)), 1),
            "timestamp":  item.get("timestamp", "")
        }, cls=DecimalEncoder)
    }
