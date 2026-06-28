const { DynamoDBClient, PutItemCommand } = require("@aws-sdk/client-dynamodb");

const db = new DynamoDBClient({});
const TABLE = process.env.TABLE_NAME;

exports.handler = async (event) => {
  const body = typeof event.body === "string" ? JSON.parse(event.body) : event.body;

  await db.send(new PutItemCommand({
    TableName: TABLE,
    Item: {
      pk:                { S: "latest" },
      timestamp:         { S: new Date().toISOString() },
      batterySoc:        { N: String(body.batterySoc) },
      batteryVoltage:    { N: String(body.batteryVoltage) },
      batteryCharge:     { N: String(body.batteryCharge) },
      acLoadCurrent:     { N: String(body.acLoadCurrent) },
      acLoadTotal:       { N: String(body.acLoadTotal) },
      solar:             { N: String(body.solar) },
      hydro:             { N: String(body.hydro) },
      diesel:            { N: String(body.diesel) },
      bufferTankCharge:  { N: String(body.bufferTankCharge) },
      flueTemp:          { N: String(body.flueTemp) },
      flowTemp:          { N: String(body.flowTemp) },
      returnTemp:        { N: String(body.returnTemp) },
    },
  }));

  return { statusCode: 200, body: "ok" };
};
