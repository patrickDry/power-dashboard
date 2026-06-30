"use client";

import { useEffect, useState } from "react";
import { PowerSystemData } from "@/lib/types";
import Card from "./Card";
import Stat from "./Stat";
import GaugeBar from "./GaugeBar";
import ProductionBar from "./ProductionBar";

function fmt(n: number, decimals = 2): string {
  return n.toFixed(decimals);
}

function socColor(pct: number): string {
  if (pct > 60) return "bg-emerald-500";
  if (pct > 30) return "bg-amber-400";
  return "bg-red-500";
}

function tankColor(pct: number): string {
  if (pct > 60) return "bg-orange-500";
  if (pct > 30) return "bg-amber-400";
  return "bg-blue-400";
}

export default function Dashboard() {
  const [data, setData] = useState<PowerSystemData | null>(null);
  const [lastUpdated, setLastUpdated] = useState<string | null>(null);
  const [error, setError] = useState<string | null>(null);
  const [relayPending, setRelayPending] = useState(false);

  const setDashboardRelay = async (state: boolean) => {
    setRelayPending(true);
    try {
      await fetch("/api/relay", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ state }),
      });
    } finally {
      setRelayPending(false);
    }
  };

  useEffect(() => {
    const es = new EventSource("/api/stream");

    es.onmessage = (e) => {
      try {
        setData(JSON.parse(e.data) as PowerSystemData);
        setLastUpdated(new Date().toLocaleTimeString());
        setError(null);
      } catch {
        setError("Received malformed data");
      }
    };

    es.onerror = () => setError("Stream disconnected — reconnecting…");

    return () => es.close();
  }, []);

  if (error && !data) {
    return (
      <div className="flex items-center justify-center h-64 text-white/40 text-sm">
        {error}
      </div>
    );
  }

  if (!data) {
    return (
      <div className="flex items-center justify-center h-64 text-white/40 text-sm animate-pulse">
        Waiting for data…
      </div>
    );
  }

  const { battery, acLoad, production, heating } = data;
  const totalProduction = production.solar + production.hydro + production.diesel;

  return (
    <div className="flex flex-col gap-6">
      {/* status bar */}
      <div className="flex items-center justify-between text-white/30 text-xs">
        <span>
          Data as of{" "}
          <span className="text-white/50 font-medium">
            {new Date(data.timestamp).toLocaleString()}
          </span>
        </span>
        {lastUpdated && <span>Refreshed {lastUpdated}</span>}
        {error && <span className="text-amber-400">{error}</span>}
      </div>

      {/* ── Battery ── */}
      <Card title="Battery" icon="🔋">
        <div className="flex flex-col gap-3">
          <div className="flex items-baseline justify-between">
            <span className="text-white text-5xl font-bold tabular-nums">
              {fmt(battery.stateOfCharge, 1)}
              <span className="text-white/40 text-2xl">%</span>
            </span>
            <div className="text-right">
              <div className="text-white/40 text-xs">State of charge</div>
            </div>
          </div>
          <GaugeBar value={battery.stateOfCharge} colorClass={socColor(battery.stateOfCharge)} height="h-4" />
          <div className="grid grid-cols-2 gap-4 pt-1">
            <Stat label="Voltage" value={fmt(battery.voltage, 1)} unit="V" />
            <Stat
              label="Power"
              value={fmt(battery.charge, 2)}
              unit="kW"
              sub={battery.charge >= 0 ? "charging" : "discharging"}
            />
          </div>
        </div>
      </Card>

      {/* ── AC Load ── */}
      <Card title="AC Load" icon="⚡">
        <div className="flex items-end justify-between">
          <div className="flex items-baseline gap-1">
            <span className="text-white text-5xl font-bold tabular-nums">{fmt(acLoad.current)}</span>
            <span className="text-white/40 text-2xl">kW</span>
          </div>
          <Stat label="Total today" value={fmt(acLoad.totalToday)} unit="kWh" />
        </div>
        <div className="flex flex-col gap-1.5">
          <div className="w-full h-4 rounded-full overflow-hidden bg-white/10">
            <div
              className="h-full rounded-full transition-all duration-500"
              style={{
                width: `${Math.min(100, (acLoad.current / 7.5) * 100)}%`,
                background: `linear-gradient(to right, #22c55e, #f97316)`,
                clipPath: "inset(0)",
              }}
            />
          </div>
          <div className="flex justify-between text-white/30 text-xs">
            <span>0 kW</span>
            <span>7.5 kW</span>
          </div>
        </div>
      </Card>

      {/* ── Production ── */}
      <Card title="Power Production" icon="🌿">
        <Stat label="Total generation" value={fmt(totalProduction)} unit="kW" />
        <ProductionBar
          total={totalProduction}
          sources={[
            { label: "Solar", value: production.solar, colorClass: "bg-yellow-400" },
            { label: "Hydro", value: production.hydro, colorClass: "bg-blue-500" },
            { label: "Diesel", value: production.diesel, colorClass: "bg-slate-500" },
          ]}
        />
      </Card>

      {/* ── Hot Water Relay ── */}
      <Card title="Hot Water Element" icon="💧">
        <div className="flex items-center justify-between">
          <div className="flex flex-col gap-0.5">
            <span className="text-white/40 text-xs">Actual state</span>
            <span className={`text-2xl font-semibold ${data.relayActualState ? "text-emerald-400" : "text-white/40"}`}>
              {data.relayActualState ? "On" : "Off"}
            </span>
          </div>
          <div className="flex flex-col items-end gap-2">
            <button
              onClick={() => setDashboardRelay(true)}
              disabled={relayPending}
              className="px-6 py-2 rounded-xl font-semibold text-sm bg-emerald-500/20 text-emerald-400 border border-emerald-500/40 hover:bg-emerald-500/30 disabled:opacity-40 disabled:cursor-not-allowed transition-all"
            >
              {relayPending ? "…" : "Turn On"}
            </button>
            <button
              onClick={() => setDashboardRelay(false)}
              disabled={relayPending}
              className="px-6 py-2 rounded-xl font-semibold text-sm bg-white/10 text-white/60 border border-white/20 hover:bg-white/20 disabled:opacity-40 disabled:cursor-not-allowed transition-all"
            >
              {relayPending ? "…" : "Turn Off"}
            </button>
          </div>
        </div>
      </Card>

      {/* ── Heating ── */}
      <Card title="Heating System" icon="🔥">
        <div className="flex flex-col gap-3">
          <div className="flex items-baseline justify-between">
            <span className="text-white text-5xl font-bold tabular-nums">
              {fmt(heating.bufferTankCharge, 1)}
              <span className="text-white/40 text-2xl">%</span>
            </span>
            <div className="text-right">
              <div className="text-white/40 text-xs">Buffer tank charge</div>
            </div>
          </div>
          <GaugeBar
            value={heating.bufferTankCharge}
            colorClass={tankColor(heating.bufferTankCharge)}
            height="h-4"
          />
          <div className="grid grid-cols-3 gap-4 pt-1">
            <Stat label="Flue temp" value={fmt(heating.flueTemperature, 1)} unit="°C" />
            <Stat label="Flow temp" value={fmt(heating.flowTemperature, 1)} unit="°C" />
            <Stat label="Return temp" value={fmt(heating.returnTemperature, 1)} unit="°C" />
          </div>
        </div>
      </Card>
    </div>
  );
}
