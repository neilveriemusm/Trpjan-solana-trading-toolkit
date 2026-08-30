# Trojan

A self-hosted Solana console for meme-coin **launch, sniping, copy trading, volume, and wallet risk**. The sidebar in the app says **Meme Tool**. This repo is the whole desk: Next.js pages on top, Solana builders underneath, no Telegram hop in the middle.

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/df9d2058-8679-4843-add4-8c2aa4335fb7" />

If you have ever juggled a launch script, a sniper process, a copy-trade bot, and a spreadsheet of wallets, this project is meant to feel like coming home. One install, one sidebar, one set of RPC and gRPC credentials.

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/970727a4-f8f1-4daf-bec9-60c74158fd8a" />

---

## Why it exists

Hosted terminals are excellent at charts. Telegram bots are excellent at “type `/buy` and go.” Neither is a kind place to **create a token, bundle the first block, fund child wallets, watch a pool, park a limit, and gather SOL back** without leaving the room.

Trojan keeps that whole loop on **your** machine:

- You choose the RPC, Yellowstone gRPC, Jito, and Astralane keys.
- Private keys are AES-encrypted in the browser before they touch `pages/api/*`.
- Orders and token records live in **your** MongoDB. Volume start/stop flags live in **your** Redis.

It is still serious infrastructure. Treat it with the same care you would give any signing service.

---

## Who it is for

| You will feel at home if… | You may want something else if… |
| --- | --- |
| You launch on pump.fun and want bundled first buys | You only want a mobile Telegram bot |
| You already pay for a fast RPC / gRPC | You expect a public SaaS with no setup |
| You want copy, snipe, limits, and volume in one UI | You only need a TradingView-style terminal |
| You are willing to run Node 20+, Mongo, and Redis | You do not want to handle private keys at all |

---

## What you can do

| Route | Kind job it does |
| --- | --- |
| `/launch` | Create a pump.fun token (name, ticker, art, socials) and open the bundle modal |
| `/bundle-buy` | Fire synchronized buys on an **existing** mint from up to 15 wallets |
| `/sniper` | Watch new Raydium (and listed DEX) pools over Yellowstone; enter with Jito + filters |
| `/copy-trade` | Mirror a target wallet; bot wallet follows via Jupiter / Jito |
| `/limit-order` | Buy or sell when market cap crosses a line; expiry window; Mongo-backed list |
| `/volume-boost` | Pick a plan, encrypt the payload, start/stop via Redis, gather SOL |
| `/trenches` | Browse fresh pump.fun / Raydium cards (liquidity, time, source) |
| `/wallet-check` | Score a counterparty and see SOL / SPL plus optional scraped traders |
| `/control/[mint]` | Per-token room: TradingView, balances, generate wallets, distribute, gather |

Wallets connect through Phantom, Solflare, Torus, or Ledger (`pages/_app.tsx`). Many automation screens still ask for a **burner private key** so the server can sign. Use dedicated wallets and small balances.

---

## A typical day

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/a5ecf608-bf4f-4a35-b393-56be2ff9f7b6" />

1. **Mint + bundle** on `/launch`. Upload art, fill website / X / Telegram / Discord, then add buyer wallets in `PumpBundleModal`. The API decrypts the payload, refuses more than 15 keys, checks SOL, and calls `PumpFunSDK` with an optional Jito send.
2. **Watch** in `/trenches` or `/control/[mint]`. Control pulls GeckoTerminal metadata, shows a TradingView widget, and lists each buyer’s SOL and SPL.
3. **Automate** with `/sniper`, `/copy-trade`, or `/limit-order` depending on whether you are hunting new pools, following a wallet, or waiting on market cap.
4. **Operate and close** with `/volume-boost` and gather/distribute on the control page. When the campaign is done, recall SOL instead of leaving dust on child keys.
5. **Check anyone** on `/wallet-check` whenever a copy target or counterparty looks off.

---

## Feature walk-through

### Launch and bundled debut

![Launch bundle](docs/launch.png)

`/launch` (and the home mint form) collect metadata and a creator key. `PumpBundleModal` then:

- Accepts many buyer secret keys and SOL amounts
- Estimates holding percent on the pump curve (`estimateHoldingPercent`)
- Lets you toggle **Jito** vs a regular send
- AES-encrypts `{ isJito, metaData, inputs }` and POSTs `/api/pump-bundle`

The handler decrypts, validates uniqueness and balances, builds buyer keypairs, and runs create + batch buy through `base/pump`. The new mint is stored on the `Token` model (`mainWalletAddress`, `tokenAddress`) so the same creator key can reopen `/control/[mint]`.

Need the same coordinated buy **after** the token already exists? Use `/bundle-buy` → `/api/bundle-buy`. Same 15-wallet ceiling, no create step.

### Sniper

`/sniper` is labeled “Snipping on Raydium” in the UI. You set:

- Buy size, slippage, Jito tip
- MEV protection toggle
- Min / max liquidity, pool supply, market cost
- DEX checkboxes: Raydium, pump.fun, Bonkfun
- Social filters: Twitter, website, Telegram
- The signing private key

`/api/sniper` hands a payload to `base/sniper.ts`, which subscribes over Yellowstone (`@triton-one/yellowstone-grpc`) to Raydium AMM accounts paired with SOL, filters brand-new pools, then `buy()` with Jito when `checkValidation` passes. Commitment is **processed** so the stream is as early as the node allows.

### Copy trading

`/copy-trade` takes a **target public key** and a **bot private key**. `/api/copy` starts `executeCopyTrade` in `base/copyTrade.ts`: a Geyser subscribe on the target, decode buy/sell, then Jupiter quotes (`getBuyTxWithJupiter` / `getSellTxWithJupiter`) and an optional Jito bundle. The UI also keeps a small list of example trader addresses you can paste from.

### Limit orders

`/limit-order` builds buy or sell brackets:

- Token lookup + `TokenInfoCard`
- Amount, private key, limit market cap (e.g. `250m`), expiry (e.g. `5d`)
- Open orders rendered as `OrderCard`s from Mongo (`models/Order.ts`)

Fields persisted: `isBuy`, `tokenAddress`, `privateKey`, `amount`, `limitMC`, `expireTime`. `base/order-monitor.ts` is the watcher that should fire when the cap is crossed.

### Volume boost

![Volume plans](docs/volume-plans.svg)

`/volume-boost` embeds a GMGN kline iframe for the mint, then lets you pick one of four plans from `volumeBoostMode` in `lib/constant.ts`. On run, the client encrypts `{ privateKey, tokenAddress, buyAmount, distributeCnt, delay }` and POSTs `/api/volume-boost`. The API sets Redis `volume-<privateKey>` to `"1"` and starts `volumeBoostingWithConstantWallet`.

Stop is `/api/stop-boosting` (flag → `"0"`). Gather is `/api/gather-sol`. The loop in `base/volume` funds child wallets, buys/sells with delays, and respects the Redis flag every round. **This is noisy on RPC.** Use a paid endpoint and burner funds only.

### Token control room

`/control/[mint]` is the after-party:

- TradingView advanced chart
- Token card from GeckoTerminal (name, MC, 24h volume, price)
- Buyer wallet table (SOL + SPL)
- Generate N new wallets, distribute SOL/tokens via `/api/distribute` (`WalletGroup` stores the generated array)
- Gather + sell via `/api/gather`
- Manual buy/sell through `TradePanel` → `/api/trade` (Pump SDK buy/sell, AES payload)

### Trenches

`/trenches` is a discovery board: `TokenCard`s with mint, symbol, source (`raydium` / pump), liquidity, and `liquidityAddedAt`. Use it to jump into sniper, copy, or control rather than as a full terminal.

### Wallet check

![Risk model](docs/risk-model.svg)

`/wallet-check` POSTs an address to `/api/wallet-check`. `base/wallet-check` pulls recent txs and token accounts, then scores:

| Feature | Why it matters |
| --- | --- |
| Failure rate | Lots of failed txs often means bots or griefing |
| Tx / hour and / day | Extreme velocity looks automated |
| Unique recipients | Spray patterns |
| Avg fee and avg transfer | Dust or whale-shaped money |
| Wallet age | BirdEye first-funded, fallback to first on-chain tx; &lt; 7 days is a flag |
| Dust token accounts | Many sub-0.01 balances |
| Large transfers | Moves &gt; 50% of current SOL |
| Scam program list | Extra points per hit (`scamAddresses` in constants) |

The UI also shows SOL, owner program, SPL list, and can scrape a trader board via `/api/scrapping`. Score is capped at **1000**. Higher means “look twice,” not a legal judgment.

---

## Architecture

![Architecture](docs/architecture.svg)

```
trojan-app/
├── pages/                 # Next.js routes + API
│   ├── launch/            # mint + bundle
│   ├── bundle-buy/
│   ├── sniper/
│   ├── copy-trade/
│   ├── limit-order/
│   ├── volume-boost/
│   ├── trenches/
│   ├── wallet-check/
│   ├── control/[payload]  # per-mint ops room
│   └── api/               # thin decrypt / validate / call base
├── components/            # Layout, sidebar, TradePanel, cards, modal
├── providers/             # theme, app chrome, wallet map, volume session
├── store/                 # Zustand
├── hooks/
├── base/                  # the actual Solana work
│   ├── pump/              # PumpFun SDK, Jito, Astralane
│   ├── volume/            # boost, distribute, gather
│   ├── sniper.ts
│   ├── copyTrade.ts
│   ├── wallet-check/
│   ├── liquidity/ · market/ · transaction/
│   └── mongodb.ts
├── models/                # Order, Token, WalletGroup
├── lib/                   # constants, Redis, types, formatters
├── docs/                  # README images
└── public/
```

**Stack:** Next.js 14 (pages router) · React 18 · HeroUI · Tailwind 3.4 · Zustand · Anchor 0.30 · Raydium SDK / SDK v2 · Jupiter lite API · Yellowstone gRPC · Jito (`jito-ts`) · MongoDB / Mongoose · Redis · CryptoJS · TradingView widgets.

---

## How a request stays kind to your keys

<img width="1536" height="1024" alt="image" src="https://github.com/user-attachments/assets/83485bf0-2027-4030-b2bb-6a7fbe8501e5" />

<img width="300" height="98" alt="image" src="https://github.com/user-attachments/assets/118bd5ed-ea04-4832-915b-1fb05f0c4013" />


1. The form never POSTs raw secrets as JSON fields for the heavy routes. It wraps them with `CryptoJS.AES.encrypt(..., NEXT_PUBLIC_ENCRYPT_KEY)`.
2. The matching `pages/api/*` handler decrypts, validates (key shape, wallet count, balances), then calls `base/`.
3. `base/` builds versioned transactions and sends them through your RPC, Jito, or Astralane.
4. Durable bits go to Mongo. Ephemeral “is volume running?” bits go to Redis.

`NEXT_PUBLIC_` means the encrypt key is available to the browser. That is a **shared passphrase**, not a vault. Anyone who can read the built frontend can encrypt/decrypt the same way. Protect the host, use HTTPS, and never commit `.env.local`.

Wallet adapter keys (Phantom, etc.) stay in the extension. Automation keys you paste are also cached in `localStorage` under `walletsByToken` (`providers/wallet.tsx`). Clear that storage on a shared machine.

---

## Why this vs other tools

![Comparison](docs/comparison.svg)

- **Telegram bots (Trojan-on-TG, BonkBot, Maestro, …)** — fastest to type, weakest as an ops desk. No launch bundle UI, no volume plans, no local risk breakdown, and the bot operator sees your flow.
- **Hosted terminals (BullX, Photon, Padre, GMGN, …)** — better charts and social tape. They do not give you a 15-wallet Jito create+buy, Redis-driven volume, or a Mongo limit book you own.
- **Raw CLI / one-off scripts** — you already have most of these primitives in `base/`. The value of this repo is the **shared providers, toasts, encryption, and sidebar** so launch and gather do not drift apart.

Trojan is the middle path: self-hosted like a script, navigable like a product.

---

## API map

| Route | Used by | Role |
| --- | --- | --- |
| `POST /api/pump-bundle` | Launch modal | Create token + bundled buys |
| `POST /api/bundle-buy` | Bundle Buys | Coordinated buys on an existing mint |
| `POST /api/sniper` | Sniper | Start gRPC watch + entry |
| `POST /api/copy` | Copy Trading | Start Geyser follow |
| `POST /api/limit-order` · `/api/order` | Limit Orders | Create / list orders |
| `POST /api/volume-boost` | Volume Boost | Start Redis-gated loop |
| `POST /api/stop-boosting` | Volume Boost | Flip the Redis flag off |
| `POST /api/gather-sol` | Volume Boost | Recall SOL from boost wallets |
| `POST /api/distribute` · `/api/gather` | Control | Fan-out / recall campaign wallets |
| `POST /api/trade` | TradePanel | Manual pump buy/sell |
| `POST /api/token` | Launch / home | List mints for a creator pubkey |
| `POST /api/ipfs-upload` | Launch | Token image |
| `POST /api/wallet-check` | Wallet Check | Features + score |
| `GET /api/scrapping` | Wallet Check | Trader board scrape |

---

## Quick start

You need **Node ≥ 20.18**, Yarn, a Solana RPC + WSS, a Yellowstone gRPC endpoint, MongoDB, and Redis.

```bash
yarn install
```

Create `.env.local` at the repo root (Next.js loads it automatically):

```env
NEXT_PUBLIC_MAIN_RPC=https://your-rpc.example
NEXT_PUBLIC_MAIN_WSS=wss://your-rpc.example
NEXT_PUBLIC_GRPC=your-grpc-host:443
NEXT_PUBLIC_GRPC_TOKEN=...
NEXT_PUBLIC_JITO_UUID=...
NEXT_PUBLIC_ASTRALANE_KEY=...
NEXT_PUBLIC_BIRD_EYE_API=...
NEXT_PUBLIC_MONGODB_URL=mongodb+srv://...
NEXT_PUBLIC_REDIS_URI=redis://...
NEXT_PUBLIC_ENCRYPT_KEY=a-long-random-passphrase
```

Then:

```bash
yarn dev
```

Open [http://localhost:3000](http://localhost:3000). Production:

```bash
yarn build
yarn start    # serves on port 3001
```

| Script | What it does |
| --- | --- |
| `yarn dev` | Next dev with `--max-old-space-size=8192` |
| `yarn build` | Production build |
| `yarn start` | Serve the build on **3001** |
| `yarn lint` | ESLint |
| `yarn test` | `ts-node test.mts` |

Optional process env used by `lib/constant.ts` and `base/`: `CHECK_IF_MINT_IS_RENOUNCED`, `CHECK_IF_MINT_IS_MUTABLE`, `CHECK_IF_MINT_IS_BURNED`, `WAIT_UNTIL_LP_IS_BURNT`, `LP_BURN_WAIT_TIME`, `AMOUNT_TO_WSOL`, `MAX_RETRY`, `FREEZE_AUTHORITY`, `IS_JITO`.

---

## Kind operational habits

- **Burner wallets only.** Paste keys you can throw away. The UI stores them in `localStorage`.
- **Small SOL.** Volume and distribute will chew fees. The plan cards show rough SOL hints (9 / 20 / 30 / 60), not guarantees.
- **Pay for RPC and gRPC.** Public mainnet endpoints will rate-limit the sniper and volume loops. Yellowstone is required for copy and snipe, not optional decoration.
- **Jito tips.** Sniper and bundles use tips (`JITO_FEE` defaults to 0.001 SOL in constants). Under-tip and you miss the block; over-tip and you donate.
- **Stop volume before you walk away.** Redis flag `"0"` is the polite off switch. Killing the Node process also works; the flag is there so the loop can exit cleanly.
- **Rotate `ENCRYPT_KEY`** if a build or `.env` ever leaked. It is a shared client/server secret, not HSM custody.
- **Do not commit** `.env.local`, key JSON dumps, or `log.txt` if it contains signatures or secrets.

---

## Tech notes

- **Pages router**, not App Router. Layout is `components/Layout/layout.tsx` (sidebar + header).
- **HeroUI** (`@heroui/react`) + Tailwind tokens (`--background`, `--accent`, theme green `#07C26B` on the top loader).
- **Pump program** id is wired in `lib/constant.ts` (`6EF8rrecthR5Dkzon8Nwu78hRvfCKubJ14M5uBEwF6P`).
- **Jupiter** lite swap API: `https://lite-api.jup.ag/swap/v1/`.
- **BirdEye** headers use `x-chain: solana` and your API key for wallet analytics.
- **Mongo models:** `Order` (limits), `Token` (creator → mint), `WalletGroup` (generated child keys for a campaign).
- **Redis key:** `volume-<privateKey>` is `"1"` while boosting.

New automation usually follows the same kindness: a page under `pages/`, a decrypting route under `pages/api/`, and the heavy lifting in `base/`.

---

## FAQ

**Does the UI name match the repo?**  
The repo and this README say Trojan. The sidebar title is “Meme Tool” and the document title is “MemeCoin Token Launch Pad.” Same app.

**Can I run this against a token I did not launch here?**  
Yes. Bundle buy, sniper, copy, limits, volume, trenches, and wallet check all take arbitrary addresses. Control works best when the mint’s buyer keys are already in `localStorage`.

**Is the trenches feed live?**  
The page is a card radar. Confirm against your own RPC / Dexscreener / GMGN before sizing up.

**Why is the heap set to 8 GB?**  
Dev and start scripts pass `--max-old-space-size=8192` because gRPC streams, volume loops, and Next’s compiler are memory-hungry together.

**Is there a license / support channel?**  
Follow the X link in the sidebar if the operator published one. This README documents the code as it sits in the tree.

---

Be gentle with keys, generous with RPC, and you will have a calmer launch desk than a folder of half-finished scripts. Welcome in.
