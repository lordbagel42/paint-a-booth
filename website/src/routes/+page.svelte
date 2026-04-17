<script lang="ts">
	import { SvelteMap } from 'svelte/reactivity';
	import { COLORS } from '$lib/colors';

	let { data } = $props();

	const CURSOR_COLORS = [
		'#E74C3C',
		'#3498DB',
		'#2ECC71',
		'#F39C12',
		'#9B59B6',
		'#1ABC9C',
		'#E67E22',
		'#EC407A'
	];

	let grid: string[] = $state(data.grid);
	let voteData: Record<number, Record<string, number>> = $state(data.voteData);
	let selectedCell: number | null = $state(null);
	let cursors = new SvelteMap<string, { cellIndex: number; color: string }>();
	let ws: WebSocket | null = $state(null);
	let connected = $state(false);
	const clientId = crypto.randomUUID().slice(0, 8);
	const cursorColor = CURSOR_COLORS[Math.floor(Math.random() * CURSOR_COLORS.length)];

	function getScore(cellIndex: number, hex: string): number {
		return voteData[cellIndex]?.[hex] ?? 0;
	}

	function getWinner(cellIndex: number): string {
		const cellVotes = voteData[cellIndex];
		if (!cellVotes) return '#000000';
		let best = '#000000';
		let bestScore = 0;
		for (const [color, score] of Object.entries(cellVotes)) {
			if (score > bestScore) {
				bestScore = score;
				best = color;
			}
		}
		return best;
	}

	function connect() {
		const protocol = location.protocol === 'https:' ? 'wss:' : 'ws:';
		const socket = new WebSocket(`${protocol}//${location.host}/api/ws`);

		socket.addEventListener('open', () => {
			connected = true;
			socket.send(JSON.stringify({ type: 'join', clientId, color: cursorColor }));
		});

		socket.addEventListener('message', (event) => {
			const msg = JSON.parse(event.data);

			if (msg.type === 'state') {
				grid = msg.grid;
				voteData = msg.voteData;
			}

			if (msg.type === 'vote') {
				const { cellIndex, color, score, winner } = msg;
				if (!voteData[cellIndex]) voteData[cellIndex] = {};
				voteData[cellIndex][color] = score;
				grid[cellIndex] = winner;
			}

			if (msg.type === 'cursor') {
				cursors.set(msg.clientId, { cellIndex: msg.cellIndex, color: msg.color });
			}

			if (msg.type === 'leave') {
				cursors.delete(msg.clientId);
			}
		});

		socket.addEventListener('close', () => {
			connected = false;
			ws = null;
			setTimeout(connect, 1000);
		});

		socket.addEventListener('error', () => {
			socket.close();
		});

		ws = socket;
	}

	function vote(cellIndex: number, color: string, direction: number) {
		if (!voteData[cellIndex]) voteData[cellIndex] = {};
		const prev = voteData[cellIndex][color] ?? 0;
		voteData[cellIndex][color] = prev + direction;
		grid[cellIndex] = getWinner(cellIndex);
		ws?.send(JSON.stringify({ type: 'vote', cellIndex, color, direction }));
	}

	let lastCursorSend = 0;
	function sendCursor(cellIndex: number) {
		const now = Date.now();
		if (now - lastCursorSend < 50) return;
		lastCursorSend = now;
		ws?.send(JSON.stringify({ type: 'cursor', cellIndex }));
	}

	function getCursorsForCell(cellIndex: number): { clientId: string; color: string }[] {
		const result: { clientId: string; color: string }[] = [];
		for (const [id, cursor] of cursors) {
			if (cursor.cellIndex === cellIndex) {
				result.push({ clientId: id, color: cursor.color });
			}
		}
		return result;
	}

	$effect(() => {
		connect();
		return () => {
			ws?.close();
		};
	});
</script>

<div class="mx-auto max-w-lg px-4 py-8">
	<h1 class="mb-2 text-center text-3xl font-bold">Paint-a-Booth</h1>
	<p class="mb-6 text-center text-sm text-neutral-500">
		{#if connected}
			<span class="inline-block h-2 w-2 rounded-full bg-green-500"></span> Connected
		{:else}
			<span class="inline-block h-2 w-2 rounded-full bg-red-500"></span> Reconnecting…
		{/if}
	</p>

	<div class="mb-6 flex justify-center">
		<div class="inline-grid grid-cols-8 overflow-hidden rounded border border-neutral-400">
			{#each grid as color, i (i)}
				{@const cellCursors = getCursorsForCell(i)}
				<button
					class="relative h-10 w-10 border border-neutral-300/50 transition-all
						hover:z-10 hover:scale-110 sm:h-12 sm:w-12"
					class:ring-2={selectedCell === i}
					class:ring-blue-500={selectedCell === i}
					class:z-20={selectedCell === i}
					style="background-color: {color}"
					onclick={() => (selectedCell = selectedCell === i ? null : i)}
					onpointerenter={() => sendCursor(i)}
					onpointerleave={() => sendCursor(-1)}
					aria-label="Cell {Math.floor(i / 8)},{i % 8}"
				>
					{#if cellCursors.length > 0}
						<div
							class="pointer-events-none absolute inset-0 flex items-center justify-center gap-0.5"
						>
							{#each cellCursors as cursor (cursor.clientId)}
								<div
									class="h-3 w-3 rounded-full border-2 border-white"
									style="background-color: {cursor.color}"
									title={cursor.clientId}
								></div>
							{/each}
						</div>
					{/if}
				</button>
			{/each}
		</div>
	</div>

	{#if selectedCell !== null}
		<div class="rounded-lg border border-neutral-300 bg-white p-4 shadow-sm">
			<div class="mb-3 flex items-center justify-between">
				<h2 class="text-lg font-semibold">
					Cell ({Math.floor(selectedCell / 8)}, {selectedCell % 8})
				</h2>
				<button
					class="text-neutral-400 hover:text-neutral-700"
					onclick={() => (selectedCell = null)}
					aria-label="Close">✕</button
				>
			</div>
			<div class="grid gap-2">
				{#each COLORS as { hex, name } (hex)}
					{@const score = getScore(selectedCell, hex)}
					<div class="flex items-center gap-3">
						<div
							class="h-6 w-6 shrink-0 rounded border border-neutral-300"
							style="background-color: {hex}"
						></div>
						<span class="w-16 text-sm">{name}</span>
						<span
							class="w-8 text-center font-mono text-sm"
							class:text-green-600={score > 0}
							class:text-red-600={score < 0}>{score}</span
						>
						<button
							type="button"
							class="rounded px-2 py-0.5 text-sm text-green-700 hover:bg-green-100"
							aria-label="Upvote {name}"
							onclick={() => vote(selectedCell!, hex, 1)}>▲</button
						>
						<button
							type="button"
							class="rounded px-2 py-0.5 text-sm text-red-700 hover:bg-red-100"
							aria-label="Downvote {name}"
							onclick={() => vote(selectedCell!, hex, -1)}>▼</button
						>
					</div>
				{/each}
			</div>
		</div>
	{/if}
</div>
