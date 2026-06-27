interface CardProps {
  title: string;
  icon?: string;
  children: React.ReactNode;
  className?: string;
}

export default function Card({ title, icon, children, className = "" }: CardProps) {
  return (
    <div className={`bg-card border border-border rounded-2xl p-5 flex flex-col gap-4 ${className}`}>
      <div className="flex items-center gap-2 text-white/50 text-xs font-semibold uppercase tracking-widest">
        {icon && <span className="text-base">{icon}</span>}
        {title}
      </div>
      {children}
    </div>
  );
}
