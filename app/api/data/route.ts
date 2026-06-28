import { DynamoDBClient, GetItemCommand } from "@aws-sdk/client-dynamodb";
import { NextResponse } from "next/server";

const db = new DynamoDBClient({
  region: process.env.AWS_REGION,
  credentials: {
    accessKeyId: process.env.AWS_ACCESS_KEY_ID!,
    secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY!,
  },
});
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
    timestamp: item.timestamp.S,
    battery: {
      stateOfCharge: Number(item.batterySoc.N),
      voltage:       Number(item.batteryVoltage.N),
      charge:        Number(item.batteryCharge.N),
    },
    acLoad: {
      current:    Number(item.acLoadCurrent.N),
      totalToday: Number(item.acLoadTotal.N),
    },
    production: {
      solar:  Number(item.solar.N),
      hydro:  Number(item.hydro.N),
      diesel: Number(item.diesel.N),
    },
    heating: {
      bufferTankCharge:  Number(item.bufferTankCharge.N),
      flueTemperature:   Number(item.flueTemp.N),
      flowTemperature:   Number(item.flowTemp.N),
      returnTemperature: Number(item.returnTemp.N),
    },
  };

  return NextResponse.json(data);
}
