# Power Dashboard

Off-grid power system monitor built with Next.js 14.

## Setup

```bash
npm install
npm run dev        # http://localhost:3000
npm run build && npm start   # production
```

Node.js 18+ required. Install from https://nodejs.org if needed.

## API

### POST /api/data

Submit a new reading from your local system. The dashboard polls this every 10 seconds.

```bash
curl -X POST http://localhost:3000/api/data \
  -H "Content-Type: application/json" \
  -d '{
    "timestamp": "2024-01-15T14:30:00Z",
    "battery": {
      "stateOfCharge": 87.5,
      "voltage": 52.4,
      "charge": 1.2
    },
    "acLoad": {
      "current": 0.85,
      "totalToday": 6.3
    },
    "production": {
      "solar": 2.1,
      "hydro": 0.8,
      "diesel": 0.0
    },
    "heating": {
      "bufferTankCharge": 72.0,
      "flueTemperature": 185.0,
      "flowTemperature": 68.0,
      "returnTemperature": 45.0
    }
  }'
```

### GET /api/data

Returns the most recent reading (used by the dashboard internally).

## Field reference

| Field | Unit | Notes |
|---|---|---|
| `battery.stateOfCharge` | % | 0–100 |
| `battery.voltage` | V | |
| `battery.charge` | kW | positive = charging, negative = discharging |
| `acLoad.current` | kW | instantaneous load |
| `acLoad.totalToday` | kWh | accumulated since midnight |
| `production.solar` | kW | |
| `production.hydro` | kW | |
| `production.diesel` | kW | |
| `heating.bufferTankCharge` | % | 0–100 |
| `heating.flueTemperature` | °C | |
| `heating.flowTemperature` | °C | |
| `heating.returnTemperature` | °C | |

## Remote access

To access the dashboard from outside your home network, either:
- Run behind a reverse proxy (nginx/Caddy) with HTTPS
- Use a tunnel like Cloudflare Tunnel or Tailscale
- Deploy to Vercel/Railway (note: in-memory store resets on restart — swap `lib/store.ts` for Redis/SQLite for persistence)
