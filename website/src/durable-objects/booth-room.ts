import { DurableObject } from 'cloudflare:workers';

const VALID_COLORS = [
	'#FF0000',
	'#FF8000',
	'#FFFF00',
	'#00FF00',
	'#0000FF',
	'#800080',
	'#FFFFFF',
	'#000000'
];

interface Attachment {
	clientId: string;
	color: string;
}

export class BoothRoom extends DurableObject {
	private votes = new Map<number, Map<string, number>>();
	private cursors = new Map<WebSocket, { clientId: string; cellIndex: number; color: string }>();
	private initialized = false;

	constructor(ctx: DurableObjectState, env: Env) {
		super(ctx, env);
		this.ctx.setWebSocketAutoResponse(new WebSocketRequestResponsePair('ping', 'pong'));
	}

	private ensureTable() {
		if (this.initialized) return;
		this.ctx.storage.sql.exec(
			`CREATE TABLE IF NOT EXISTS pixel_vote (
				cell_index INTEGER NOT NULL,
				color TEXT NOT NULL,
				score INTEGER NOT NULL DEFAULT 0,
				PRIMARY KEY(cell_index, color)
			)`
		);
		this.initialized = true;
	}

	private loadVotes() {
		this.ensureTable();
		this.votes.clear();
		const rows = this.ctx.storage.sql.exec('SELECT cell_index, color, score FROM pixel_vote');
		for (const row of rows) {
			const cellIndex = row.cell_index as number;
			const color = row.color as string;
			const score = row.score as number;
			if (!this.votes.has(cellIndex)) this.votes.set(cellIndex, new Map());
			this.votes.get(cellIndex)!.set(color, score);
		}
	}

	private getGrid(): string[] {
		const grid: string[] = new Array(64).fill('#000000');
		for (const [cellIndex, colorMap] of this.votes) {
			let bestColor = '#000000';
			let bestScore = 0;
			for (const [color, score] of colorMap) {
				if (score > bestScore) {
					bestScore = score;
					bestColor = color;
				}
			}
			if (bestScore > 0) grid[cellIndex] = bestColor;
		}
		return grid;
	}

	private getVoteData(): Record<number, Record<string, number>> {
		const data: Record<number, Record<string, number>> = {};
		for (const [cellIndex, colorMap] of this.votes) {
			data[cellIndex] = {};
			for (const [color, score] of colorMap) {
				data[cellIndex][color] = score;
			}
		}
		return data;
	}

	private broadcast(message: string, exclude?: WebSocket) {
		for (const ws of this.ctx.getWebSockets()) {
			if (ws !== exclude) {
				try {
					ws.send(message);
				} catch {
					// connection dead, will be cleaned up by webSocketClose
				}
			}
		}
	}

	async fetch(request: Request): Promise<Response> {
		const url = new URL(request.url);

		if (url.pathname === '/state') {
			this.loadVotes();
			return Response.json({ grid: this.getGrid(), voteData: this.getVoteData() });
		}

		if (url.pathname === '/grid') {
			this.loadVotes();
			const grid = this.getGrid();
			const body = grid
				.map((hex) => {
					const r = parseInt(hex.slice(1, 3), 16);
					const g = parseInt(hex.slice(3, 5), 16);
					const b = parseInt(hex.slice(5, 7), 16);
					return (
						String(r).padStart(3, '0') +
						'.' +
						String(g).padStart(3, '0') +
						'.' +
						String(b).padStart(3, '0')
					);
				})
				.join(',');
			return new Response(body, { headers: { 'Content-Type': 'text/plain' } });
		}

		const upgradeHeader = request.headers.get('Upgrade');
		if (upgradeHeader !== 'websocket') {
			return new Response('Expected WebSocket', { status: 426 });
		}

		const pair = new WebSocketPair();
		const [client, server] = Object.values(pair);
		this.ctx.acceptWebSocket(server);

		this.loadVotes();
		server.send(
			JSON.stringify({
				type: 'state',
				grid: this.getGrid(),
				voteData: this.getVoteData()
			})
		);

		return new Response(null, { status: 101, webSocket: client });
	}

	async webSocketMessage(ws: WebSocket, message: string | ArrayBuffer) {
		if (typeof message !== 'string') return;

		let data: Record<string, unknown>;
		try {
			data = JSON.parse(message);
		} catch {
			return;
		}

		if (data.type === 'join') {
			const clientId = String(data.clientId ?? '');
			const color = String(data.color ?? '');
			if (!clientId) return;
			const attachment: Attachment = { clientId, color };
			ws.serializeAttachment(attachment);
			this.cursors.set(ws, { clientId, cellIndex: -1, color });
			return;
		}

		if (data.type === 'vote') {
			const cellIndex = Number(data.cellIndex);
			const color = String(data.color ?? '');
			const direction = Number(data.direction);
			if (isNaN(cellIndex) || cellIndex < 0 || cellIndex > 63) return;
			if (!VALID_COLORS.includes(color)) return;
			if (direction !== 1 && direction !== -1) return;

			this.ensureTable();
			this.ctx.storage.sql.exec(
				`INSERT INTO pixel_vote (cell_index, color, score)
				 VALUES (?, ?, ?)
				 ON CONFLICT(cell_index, color)
				 DO UPDATE SET score = score + ?`,
				cellIndex,
				color,
				direction,
				direction
			);

			if (!this.votes.has(cellIndex)) this.votes.set(cellIndex, new Map());
			const colorMap = this.votes.get(cellIndex)!;
			colorMap.set(color, (colorMap.get(color) ?? 0) + direction);
			const newScore = colorMap.get(color)!;

			let winner = '#000000';
			let bestScore = 0;
			for (const [c, s] of colorMap) {
				if (s > bestScore) {
					bestScore = s;
					winner = c;
				}
			}

			this.broadcast(
				JSON.stringify({
					type: 'vote',
					cellIndex,
					color,
					score: newScore,
					winner
				})
			);
			return;
		}

		if (data.type === 'cursor') {
			const cellIndex = Number(data.cellIndex);
			if (isNaN(cellIndex) || cellIndex < -1 || cellIndex > 63) return;
			const cursor = this.cursors.get(ws);
			if (!cursor) return;
			cursor.cellIndex = cellIndex;
			this.broadcast(
				JSON.stringify({
					type: 'cursor',
					clientId: cursor.clientId,
					cellIndex,
					color: cursor.color
				}),
				ws
			);
			return;
		}
	}

	async webSocketClose(ws: WebSocket, code: number) {
		const cursor = this.cursors.get(ws);
		if (cursor) {
			this.broadcast(JSON.stringify({ type: 'leave', clientId: cursor.clientId }));
			this.cursors.delete(ws);
		}
		ws.close(code, 'DO closing');
	}

	async webSocketError(ws: WebSocket) {
		const cursor = this.cursors.get(ws);
		if (cursor) {
			this.broadcast(JSON.stringify({ type: 'leave', clientId: cursor.clientId }));
			this.cursors.delete(ws);
		}
	}
}
