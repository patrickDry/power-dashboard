import { DynamoDBClient, GetItemCommand } from "@aws-sdk/client-dynamodb";
import { NextResponse } from "next/server";

const db = new DynamoDBClient({ region: process.env.AWS_REGION });
const TABLE = process.env.TABLE_NAME!;

export async function GET() {
  const result = await db.send(new GetItemCommand({
    TableName: TABLE,
    Key: { pk: { S: "latest" } },
  }));

  if (!result.Item) {
    return NextResponse.json({ error: "No data received yet" }, { status: 404 });
  }

  const item = result.Item;
  const data = {
    timestamp:        item.timestamp.S,
    batterySoc:       Number(item.batterySoc.N),
    batteryVoltage:   Number(item.batteryVoltage.N),
    batteryCharge:    Number(item.batteryCharge.N),
    acLoadCurrent:    Number(item.acLoadCurrent.N),
    acLoadTotal:      Number(item.acLoadTotal.N),
    solar:            Number(item.solar.N),
    hydro:            Number(item.hydro.N),
    diesel:           Number(item.diesel.N),
    bufferTankCharge: Number(item.bufferTankCharge.N),
    flueTemp:         Number(item.flueTemp.N),
    flowTemp:         Number(item.flowTemp.N),
    returnTemp:       Number(item.returnTemp.N),
  };

  return NextResponse.json(data);
}
