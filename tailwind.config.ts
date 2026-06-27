import type { Config } from "tailwindcss";

const config: Config = {
  content: [
    "./app/**/*.{js,ts,jsx,tsx,mdx}",
    "./components/**/*.{js,ts,jsx,tsx,mdx}",
  ],
  theme: {
    extend: {
      colors: {
        surface: "#0f1117",
        card: "#1a1d27",
        border: "#2a2d3a",
      },
    },
  },
  plugins: [],
};

export default config;
