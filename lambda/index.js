const {
  DynamoDBClient,
  PutItemCommand,
  GetItemCommand,
  UpdateItemCommand,
} = require("@aws-sdk/client-dynamodb");

const db = new DynamoDBClient({});
const TABLE = process.env.TABLE_NAME;

exports.handler = async (event) => {
  const body = typeof event.body === "string" ? JSON.parse(event.body) : event.body;

  // 1. Store the latest sensor reading. This PutItem replaces the whole "latest"
  //    item, which is why the relay command is kept in a SEPARATE "relay" item.
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
      relayActualState:  { BOOL: body.relayActualState === true },
    },
  }));

  // 2. Read any pending relay command set by the dashboard.
  const relay = await db.send(new GetItemCommand({
    TableName: TABLE,
    Key: { pk: { S: "relay" } },
  }));

  const changed = relay.Item?.relayDashboardChanged?.BOOL ?? false;
  const state   = relay.Item?.relayDashboardState?.BOOL ?? false;

  // 3. Consume the one-shot: if there was a pending change, clear the flag now
  //    so the Arduino only acts on it once.
  if (changed) {
    await db.send(new UpdateItemCommand({
      TableName: TABLE,
      Key: { pk: { S: "relay" } },
      UpdateExpression: "SET relayDashboardChanged = :false",
      ExpressionAttributeValues: { ":false": { BOOL: false } },
    }));
  }

  // 4. Hand the command back to the Arduino in the response to its own push.
  return {
    statusCode: 200,
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ relayDashboardChanged: changed, relayDashboardState: state }),
  };
};
