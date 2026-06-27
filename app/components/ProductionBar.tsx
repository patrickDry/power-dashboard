"use client";

interface Source {
  label: string;
  value: number;
  colorClass: string;
}

interface ProductionBarProps {
  sources: Source[];
  total: number;
}

export default function ProductionBar({ sources, total }: ProductionBarProps) {
  return (
    <div className="flex flex-col gap-3">
      {/* stacked bar */}
      <div className="flex w-full h-4 rounded-full overflow-hidden gap-0.5 bg-white/10">
        {sources.map((s) => {
          const pct = total > 0 ? (s.value / total) * 100 : 0;
          return (
            <div
              key={s.label}
              className={`${s.colorClass} transition-all duration-500`}
              style={{ width: `${pct}%` }}
            />
          );
        })}
      </div>
      {/* legend */}
      <div className="flex flex-wrap gap-4">
        {sources.map((s) => (
          <div key={s.label} className="flex items-center gap-1.5">
            <span className={`w-2.5 h-2.5 rounded-sm ${s.colorClass}`} />
            <span className="text-white/50 text-xs">{s.label}</span>
            <span className="text-white text-xs font-semibold tabular-nums">
              {s.value.toFixed(2)} kW
            </span>
          </div>
        ))}
      </div>
    </div>
  );
}
