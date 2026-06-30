import { DynamoDBClient, GetItemCommand, UpdateItemCommand } from "@aws-sdk/client-dynamodb";
import { NextRequest, NextResponse } from "next/server";

const db = new DynamoDBClient({
  region: process.env.AWS_REGION,
  credentials: {
    accessKeyId: process.env.AWS_ACCESS_KEY_ID!,
    secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY!,
  },
});
const TABLE = process.env.TABLE_NAME!;

// Arduino polls this — returns relayDashboardChanged and relayDashboardState
export async function GET() {
  const result = await db.send(new GetItemCommand({
    TableName: TABLE,
    Key: { pk: { S: "latest" } },
  }));

  const changed = result.Item?.relayDashboardChanged?.BOOL ?? false;
  const state   = result.Item?.relayDashboardState?.BOOL ?? false;

  return NextResponse.json({ relayDashboardChanged: changed, relayDashboardState: state });
}

// Arduino calls this to clear the changed flag after acting on it
export async function DELETE() {
  await db.send(new UpdateItemCommand({
    TableName: TABLE,
    Key: { pk: { S: "latest" } },
    UpdateExpression: "SET relayDashboardChanged = :false",
    ExpressionAttributeValues: { ":false": { BOOL: false } },
  }));

  return NextResponse.json({ ok: true });
}

// Dashboard calls this to request a relay state change.
// Requires the correct password — checked server-side so it never ships to the client.
export async function POST(req: NextRequest) {
  const { state, password } = await req.json() as { state: boolean; password?: string };

  const expected = process.env.RELAY_PASSWORD;
  if (!expected || password !== expected) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await db.send(new UpdateItemCommand({
    TableName: TABLE,
    Key: { pk: { S: "latest" } },
    UpdateExpression: "SET relayDashboardState = :state, relayDashboardChanged = :true",
    ExpressionAttributeValues: {
      ":state": { BOOL: state },
      ":true":  { BOOL: true },
    },
  }));

  return NextResponse.json({ ok: true });
}
