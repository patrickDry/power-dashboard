import { getData } from "@/lib/store";

export const runtime = "edge";
export const dynamic = "force-dynamic";

const POLL_MS = 5_000;

export async function GET() {
  const stream = new ReadableStream({
    async start(controller) {
      const encode = (data: object) =>
        new TextEncoder().encode(`data: ${JSON.stringify(data)}\n\n`);

      let lastTimestamp: string | null = null;

      const tick = async () => {
        try {
          const data = await getData();
          if (data && data.timestamp !== lastTimestamp) {
            lastTimestamp = data.timestamp;
            controller.enqueue(encode(data));
          }
        } catch {
          // KV error — skip this tick, try again next interval
        }
      };

      // Send current data immediately
      await tick();

      // Poll KV every POLL_MS and push only when data changes
      const id = setInterval(tick, POLL_MS);

      // Clean up when client disconnects
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
