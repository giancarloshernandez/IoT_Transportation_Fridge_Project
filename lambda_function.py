import boto3
import json

sns = boto3.client('sns')

def lambda_handler(event, context):
    temp = event.get('t', 'N/A')
    humid = event.get('h', 'N/A')
    lat = event.get('lat', 'Unknown')
    lng = event.get('lng', 'Unknown')
    spd = event.get('spd', 0)
    
    # Check specific alarms
    alarms = []
    if event.get('tA') == 1: alarms.append("HIGH TEMPERATURE")
    if event.get('hA') == 1: alarms.append("HIGH HUMIDITY")
    if event.get('dA') == 1: alarms.append("DOOR OPENED")
    if event.get('mA') == 1: alarms.append("HARSH MOVEMENT")

    msg = (
        f"TRANSPORTATION SYSTEM ALERT \n"
        f"--------------------------------\n"
        f"STATUS: {', '.join(alarms) if alarms else 'General Anomaly'}\n\n"
        f"Environmental Data:\n"
        f"- Temperature: {temp}°C\n"
        f"- Humidity: {humid}%\n\n"
        f"Vehicle Location:\n"
        f"- Coordinates: {lat}, {lng}\n"
        f"- View on Map: https://www.google.com/maps?q={lat},{lng}\n"
        f"--------------------------------"
    )
    
    sns.publish(
        TopicArn='arn:aws:sns:us-east-2:808715035840:HVAC_Project_Alarms',
        Message=msg,
        Subject='Truck Security Alert'
    )
    
    return {
        'statusCode': 200,
        'body': json.dumps('Alert sent successfully')
    }
