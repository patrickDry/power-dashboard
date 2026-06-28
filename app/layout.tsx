import type { Metadata } from "next";
import "./globals.css";

export const metadata: Metadata = {
  title: "Power Dashboard",
  description: "Off-grid power system monitor",
  icons: {
    icon: "data:image/svg+xml,<svg xmlns='http://www.w3.org/2000/svg' viewBox='0 0 100 100'><text y='.9em' font-size='90'>⚡</text></svg>",
  },
};

export default function RootLayout({ children }: { children: React.ReactNode }) {
  return (
    <html lang="en">
      <body className="min-h-screen bg-surface antialiased">
        <header className="sticky top-0 z-10 bg-surface/80 backdrop-blur border-b border-border px-4 py-3">
          <div className="max-w-lg mx-auto flex items-center gap-2">
            <span className="text-xl">⚡</span>
            <h1 className="text-white font-semibold text-lg tracking-tight">Power Dashboard</h1>
          </div>
        </header>
        <main className="max-w-lg mx-auto px-4 py-6">{children}</main>
      </body>
    </html>
  );
}
