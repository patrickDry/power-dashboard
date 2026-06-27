interface StatProps {
  label: string;
  value: string | number;
  unit?: string;
  sub?: string;
}

export default function Stat({ label, value, unit, sub }: StatProps) {
  return (
    <div className="flex flex-col gap-0.5">
      <span className="text-white/40 text-xs">{label}</span>
      <div className="flex items-baseline gap-1">
        <span className="text-white text-2xl font-semibold tabular-nums">{value}</span>
        {unit && <span className="text-white/50 text-sm">{unit}</span>}
      </div>
      {sub && <span className="text-white/30 text-xs">{sub}</span>}
    </div>
  );
}
