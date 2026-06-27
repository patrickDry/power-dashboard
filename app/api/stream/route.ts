import { DynamoDBClient, GetItemCommand } from "@aws-sdk/client-dynamodb";

export const runtime = "edge";
export const dynamic = "force-dynamic";

const POLL_MS = 5_000;

async function getLatest() {
  const db = new DynamoDBClient({ region: process.env.AWS_REGION });
  const result = await db.send(new GetItemCommand({
    TableName: process.env.TABLE_NAME!,
    Key: { pk: { S: "latest" } },
  }));

  if (!result.Item) return null;
  const item = result.Item;
  return {
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
}

export async function GET() {
  const stream = new ReadableStream({
    async start(controller) {
      const encode = (data: object) =>
        new TextEncoder().encode(`data: ${JSON.stringify(data)}\n\n`);

      let lastTimestamp: string | null | undefined = null;

      const tick = async () => {
        try {
          const data = await getLatest();
          if (data && data.timestamp !== lastTimestamp) {
            lastTimestamp = data.timestamp;
            controller.enqueue(encode(data));
          }
        } catch {
          // skip this tick on error
        }
      };

      await tick();
      const id = setInterval(tick, POLL_MS);
      return () => clearInterval(id);
    },
  });

  return new Response(stream, {
    headers: {
      "Content-Type": "text/event-stream",
      "Cache-Control": "no-cache",
      Connection: "keep-alive",
    },
  });
}
