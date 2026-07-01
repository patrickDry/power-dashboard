import { DynamoDBClient, UpdateItemCommand } from "@aws-sdk/client-dynamodb";
import { NextRequest, NextResponse } from "next/server";

const db = new DynamoDBClient({
  region: process.env.AWS_REGION,
  credentials: {
    accessKeyId: process.env.AWS_ACCESS_KEY_ID!,
    secretAccessKey: process.env.AWS_SECRET_ACCESS_KEY!,
  },
});
const TABLE = process.env.TABLE_NAME!;

// Dashboard sets the desired relay state. Stored in a separate "relay" item so
// the Arduino's 5-second sensor PutItem (on the "latest" item) never wipes it.
// The Arduino picks this up in the response to its next /data push, and the
// Lambda clears the changed flag once it's been handed over (one-shot).
//
// Password is verified server-side so it never ships to the browser bundle.
export async function POST(req: NextRequest) {
  const { state, password } = await req.json() as { state: boolean; password?: string };

  const expected = process.env.RELAY_PASSWORD;
  if (!expected || password !== expected) {
    return NextResponse.json({ error: "Unauthorized" }, { status: 401 });
  }

  await db.send(new UpdateItemCommand({
    TableName: TABLE,
    Key: { pk: { S: "relay" } },
    UpdateExpression: "SET relayDashboardState = :state, relayDashboardChanged = :true",
    ExpressionAttributeValues: {
      ":state": { BOOL: state },
      ":true":  { BOOL: true },
    },
  }));

  return NextResponse.json({ ok: true });
}
