import { DynamoDBClient, PutItemCommand } from "@aws-sdk/client-dynamodb";

const db = new DynamoDBClient({});
const TABLE = process.env.TABLE_NAME;  // set in Lambda env vars

// IoT Core rule delivers the MQTT payload as a parsed object directly
export const handler = async (event) => {
  await db.send(new PutItemCommand({
    TableName: TABLE,
    Item: {
      pk:                { S: "latest" },
      timestamp:         { S: new Date().toISOString() },
      batterySoc:        { N: String(event.batterySoc) },
      batteryVoltage:    { N: String(event.batteryVoltage) },
      batteryCharge:     { N: String(event.batteryCharge) },
      acLoadCurrent:     { N: String(event.acLoadCurrent) },
      acLoadTotal:       { N: String(event.acLoadTotal) },
      solar:             { N: String(event.solar) },
      hydro:             { N: String(event.hydro) },
      diesel:            { N: String(event.diesel) },
      bufferTankCharge:  { N: String(event.bufferTankCharge) },
      flueTemp:          { N: String(event.flueTemp) },
      flowTemp:          { N: String(event.flowTemp) },
      returnTemp:        { N: String(event.returnTemp) },
    },
  }));
};
