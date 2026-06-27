"use client";

interface GaugeBarProps {
  value: number;      // 0–100
  colorClass?: string;
  height?: string;
}

export default function GaugeBar({
  value,
  colorClass = "bg-emerald-500",
  height = "h-3",
}: GaugeBarProps) {
  const pct = Math.min(100, Math.max(0, value));
  return (
    <div className={`w-full ${height} rounded-full bg-white/10 overflow-hidden`}>
      <div
        className={`${height} rounded-full transition-all duration-500 ${colorClass}`}
        style={{ width: `${pct}%` }}
      />
    </div>
  );
}
