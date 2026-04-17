<script lang="ts">
	import { SvelteMap, SvelteSet } from 'svelte/reactivity';
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
	let cursors = new SvelteMap<string, { x: number; y: number; color: string }>();
	let ws: WebSocket | null = $state(null);
	let connected = $state(false);
	const clientId = crypto.randomUUID().slice(0, 8);
	const cursorColor = CURSOR_COLORS[Math.floor(Math.random() * CURSOR_COLORS.length)];

	// Drag-to-paint state
	let brushColor: string = $state(COLORS[0].hex);
	let isPainting = $state(false);
	let paintedCells = new SvelteSet<number>();
	let copyMessage = $state('');

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
				cursors.set(msg.clientId, { x: msg.x, y: msg.y, color: msg.color });
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

	function paintCell(cellIndex: number) {
		if (paintedCells.has(cellIndex)) return;
		paintedCells.add(cellIndex);
		vote(cellIndex, brushColor, 1);
	}

	function onCellPointerDown(cellIndex: number, e: PointerEvent) {
		if (e.button !== 0) return;
		isPainting = true;
		paintedCells = new SvelteSet();
		paintCell(cellIndex);
	}

	function onCellPointerEnter(cellIndex: number) {
		if (isPainting) paintCell(cellIndex);
	}

	let gridEl: HTMLDivElement | undefined = $state();
	let lastCursorSend = 0;
	function sendCursor(e: PointerEvent) {
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

	async function copyGrid() {
		await navigator.clipboard.writeText(JSON.stringify(grid));
		copyMessage = 'Copied!';
		setTimeout(() => (copyMessage = ''), 2000);
	}

	async function pasteGrid() {
		try {
			const text = await navigator.clipboard.readText();
			const parsed = JSON.parse(text);
			if (!Array.isArray(parsed) || parsed.length !== 64) {
				copyMessage = 'Invalid grid data';
				setTimeout(() => (copyMessage = ''), 2000);
				return;
			}
			for (let i = 0; i < 64; i++) {
				const color = parsed[i];
				if (typeof color === 'string' && color.startsWith('#') && color !== grid[i]) {
					vote(i, color, 1);
				}
			}
			copyMessage = 'Pasted!';
			setTimeout(() => (copyMessage = ''), 2000);
		} catch {
			copyMessage = 'Paste failed';
			setTimeout(() => (copyMessage = ''), 2000);
		}
	}

	$effect(() => {
		connect();
		return () => {
			ws?.close();
		};
	});

	$effect(() => {
		function onPointerUp() {
			isPainting = false;
		}
		window.addEventListener('pointerup', onPointerUp);
		return () => window.removeEventListener('pointerup', onPointerUp);
	});
</script>

<div class="mx-auto max-w-lg px-4 py-8 select-none">
	<h1 class="mb-2 text-center text-3xl font-bold">Paint-a-Booth</h1>
	<p class="mb-4 text-center text-sm text-neutral-500">
		{#if connected}
			<span class="inline-block h-2 w-2 rounded-full bg-green-500"></span> Connected
		{:else}
			<span class="inline-block h-2 w-2 rounded-full bg-red-500"></span> Reconnecting…
		{/if}
	</p>

	<!-- Brush palette -->
	<div class="mb-3 flex flex-wrap items-center justify-center gap-2">
		<span class="text-xs font-medium text-neutral-500">Brush:</span>
		{#each COLORS as { hex, name } (hex)}
			<button
				class="h-7 w-7 rounded border-2 transition-transform hover:scale-110"
				class:border-blue-500={brushColor === hex}
				class:scale-110={brushColor === hex}
				class:border-neutral-300={brushColor !== hex}
				style="background-color: {hex}"
				onclick={() => (brushColor = hex)}
				aria-label="Select {name} brush"
				title={name}
			></button>
		{/each}
	</div>

	<!-- Copy/Paste buttons -->
	<div class="mb-4 flex items-center justify-center gap-2">
		<button
			class="rounded border border-neutral-300 px-3 py-1 text-xs hover:bg-neutral-100"
			onclick={copyGrid}
		>
			📋 Copy Grid
		</button>
		<button
			class="rounded border border-neutral-300 px-3 py-1 text-xs hover:bg-neutral-100"
			onclick={pasteGrid}
		>
			📥 Paste Grid
		</button>
		{#if copyMessage}
			<span class="text-xs text-neutral-500">{copyMessage}</span>
		{/if}
	</div>

	<!-- Grid -->
	<div class="relative mb-6 flex justify-center">
		<div
			bind:this={gridEl}
			class="relative inline-grid touch-none grid-cols-8 overflow-visible rounded border border-neutral-400"
			onpointermove={sendCursor}
			onpointerleave={sendCursorLeave}
		>
			{#each grid as color, i (i)}
				<button
					class="h-10 w-10 border border-neutral-300/50 sm:h-12 sm:w-12"
					class:ring-2={selectedCell === i}
					class:ring-blue-500={selectedCell === i}
					class:z-20={selectedCell === i}
					style="background-color: {color}"
					onpointerdown={(e) => onCellPointerDown(i, e)}
					onpointerenter={() => onCellPointerEnter(i)}
					onclick={() => (selectedCell = selectedCell === i ? null : i)}
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
							width="20"
							height="24"
							viewBox="0 0 20 24"
							fill="none"
							xmlns="http://www.w3.org/2000/svg"
							class="drop-shadow"
						>
							<path
								d="M2 1L18 12L10 13L7 22L2 1Z"
								fill={cursor.color}
								stroke="white"
								stroke-width="1.5"
								stroke-linejoin="round"
							/>
						</svg>
						<span
							class="absolute top-5 left-3 rounded-full px-1.5 py-0.5 text-[9px] font-medium whitespace-nowrap text-white shadow"
							style="background-color: {cursor.color}">{id.slice(0, 4)}</span
						>
					</div>
				{/if}
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
