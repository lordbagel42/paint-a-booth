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
	let cursors = new SvelteMap<string, { x: number; y: number; color: string; userName?: string }>();
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
			socket.send(
				JSON.stringify({ type: 'join', clientId, color: cursorColor, userName: data.user?.name })
			);
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
				cursors.set(msg.clientId, { x: msg.x, y: msg.y, color: msg.color, userName: msg.userName });
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
		if (!data.user) return;
		if (!voteData[cellIndex]) voteData[cellIndex] = {};
		const prev = voteData[cellIndex][color] ?? 0;
		voteData[cellIndex][color] = prev + direction;
		grid[cellIndex] = getWinner(cellIndex);
		ws?.send(JSON.stringify({ type: 'vote', cellIndex, color, direction }));
		fetch('/api/vote-log', {
			method: 'POST',
			headers: { 'Content-Type': 'application/json' },
			body: JSON.stringify({ cellIndex, color, direction })
		});
	}
	let gridEl: HTMLDivElement | undefined = $state();
	let lastCursorSend = 0;
	function sendCursor(e: PointerEvent) {
		if (!data.user) return;
		const now = Date.now();
		if (now - lastCursorSend < 33) return;
		if (!gridEl) return;
		lastCursorSend = now;
		const rect = gridEl.getBoundingClientRect();
		const x = (e.clientX - rect.left) / rect.width;
		const y = (e.clientY - rect.top) / rect.height;
		ws?.send(JSON.stringify({ type: 'cursor', x, y }));
	}

	function sendCursorLeave() {
		ws?.send(JSON.stringify({ type: 'cursor', x: -1, y: -1 }));
	}
	$effect(() => {
		connect();
		return () => {
			ws?.close();
		};
	});
</script>

<div class="mx-auto max-w-lg px-4 py-10 select-none">
	<div class="mb-6 flex items-center justify-between">
		<h1 class="font-mono text-lg font-bold tracking-tight">paint-a-booth</h1>
		<span class="flex items-center gap-2 font-mono text-xs text-neutral-500">
			{#if connected}
				<span class="h-1.5 w-1.5 bg-green-400"></span>live
			{:else}
				<span class="h-1.5 w-1.5 bg-red-400"></span>reconnecting
			{/if}
		</span>
	</div>

	<!-- Grid -->
	<div class="relative mb-6 flex justify-center">
		<div
			bind:this={gridEl}
			class="relative inline-grid touch-none grid-cols-8 overflow-visible border border-neutral-800"
			onpointermove={sendCursor}
			onpointerleave={sendCursorLeave}
		>
			{#each grid as color, i (i)}
				<button
					class="h-10 w-10 border border-neutral-800 transition-[border-color] hover:border-neutral-500 sm:h-12 sm:w-12"
					class:border-white={selectedCell === i}
					class:z-20={selectedCell === i}
					style="background-color: {color}"
					onclick={() => {
						if (!data.user) return;
						selectedCell = selectedCell === i ? null : i;
					}}
					aria-label="Cell {Math.floor(i / 8)},{i % 8}"
				></button>
			{/each}

			<!-- Remote cursors -->
			{#each [...cursors] as [id, cursor] (id)}
				{#if cursor.x >= 0 && cursor.y >= 0}
					<div
						class="pointer-events-none absolute z-50"
						style="left: {cursor.x * 100}%; top: {cursor.y *
							100}%; transition: left 80ms linear, top 80ms linear;"
					>
						<svg
							width="18"
							height="22"
							viewBox="0 0 20 24"
							fill="none"
							xmlns="http://www.w3.org/2000/svg"
						>
							<path
								d="M2 1L18 12L10 13L7 22L2 1Z"
								fill={cursor.color}
								stroke="#0a0a0a"
								stroke-width="1.5"
								stroke-linejoin="round"
							/>
						</svg>
						<span
							class="absolute top-4 left-3 px-1 py-px font-mono text-[8px] whitespace-nowrap text-white"
							style="background-color: {cursor.color}">{cursor.userName || id.slice(0, 4)}</span
						>
					</div>
				{/if}
			{/each}
		</div>
	</div>

	{#if selectedCell !== null}
		<div class="border border-neutral-800 bg-[#111] p-4">
			<div class="mb-3 flex items-center justify-between">
				<h2 class="font-mono text-sm text-neutral-400">
					cell[{selectedCell}]
					<span class="text-neutral-600">({Math.floor(selectedCell / 8)}, {selectedCell % 8})</span>
				</h2>
				<button
					class="font-mono text-xs text-neutral-600 hover:text-white"
					onclick={() => (selectedCell = null)}
					aria-label="Close">✕</button
				>
			</div>
			<div class="grid gap-1">
				{#each COLORS as { hex, name } (hex)}
					{@const score = getScore(selectedCell, hex)}
					<div
						class="flex items-center gap-3 border border-transparent px-2 py-1 hover:border-neutral-800"
					>
						<div
							class="h-5 w-5 shrink-0 border border-neutral-700"
							style="background-color: {hex}"
						></div>
						<span class="w-14 font-mono text-xs text-neutral-400">{name}</span>
						<span
							class="w-8 text-center font-mono text-xs"
							class:text-green-400={score > 0}
							class:text-red-400={score < 0}
							class:text-neutral-600={score === 0}>{score}</span
						>
						<button
							type="button"
							class="px-2 py-0.5 font-mono text-xs text-neutral-500 hover:bg-neutral-800 hover:text-white"
							aria-label="Upvote {name}"
							onclick={() => vote(selectedCell!, hex, 1)}>+</button
						>
						<button
							type="button"
							class="px-2 py-0.5 font-mono text-xs text-neutral-500 hover:bg-neutral-800 hover:text-white"
							aria-label="Downvote {name}"
							onclick={() => vote(selectedCell!, hex, -1)}>−</button
						>
					</div>
				{/each}
			</div>
		</div>
	{/if}
</div>
