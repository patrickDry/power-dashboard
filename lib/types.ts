export interface PowerSystemData {
  timestamp: string;
  battery: {
    stateOfCharge: number;  // 0–100 %
    voltage: number;         // V
    charge: number;          // kW (positive = charging, negative = discharging)
  };
  acLoad: {
    current: number;    // kW right now
    totalToday: number; // kWh accumulated today
  };
  production: {
    solar: number;  // kW
    hydro: number;  // kW
    diesel: number; // kW
  };
  heating: {
    bufferTankCharge: number;    // 0–100 %
    flueTemperature: number;     // °C
    flowTemperature: number;     // °C
    returnTemperature: number;   // °C
  };
}
