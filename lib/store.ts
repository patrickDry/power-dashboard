import { kv } from "@vercel/kv";
import { PowerSystemData } from "./types";

const KEY = "power:latest";

export async function setData(data: PowerSystemData): Promise<void> {
  await kv.set(KEY, data);
}

export async function getData(): Promise<PowerSystemData | null> {
  return kv.get<PowerSystemData>(KEY);
}
