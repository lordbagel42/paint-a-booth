<script lang="ts">
	import { enhance } from '$app/forms';
	import { invalidateAll } from '$app/navigation';
	import { COLORS } from '$lib/colors';

	let { data } = $props();

	let selectedCell: number | null = $state(null);

	function getScore(cellIndex: number, hex: string): number {
		return data.voteData[cellIndex]?.[hex] ?? 0;
	}

	$effect(() => {
		const interval = setInterval(() => invalidateAll(), 2000);
		return () => clearInterval(interval);
	});
</script>

<div class="mx-auto max-w-lg px-4 py-8">
	<h1 class="mb-6 text-center text-3xl font-bold">Paint-a-Booth</h1>

	<div class="mb-6 flex justify-center">
		<div class="inline-grid grid-cols-8 overflow-hidden rounded border border-neutral-400">
			{#each data.grid as color, i (i)}
				<button
					class="h-10 w-10 border border-neutral-300/50 transition-all
						hover:z-10 hover:scale-110 sm:h-12 sm:w-12"
					class:ring-2={selectedCell === i}
					class:ring-blue-500={selectedCell === i}
					class:z-20={selectedCell === i}
					style="background-color: {color}"
					onclick={() => (selectedCell = selectedCell === i ? null : i)}
					aria-label="Cell {Math.floor(i / 8)},{i % 8}"
				></button>
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
						<form method="POST" action="?/vote" use:enhance class="contents">
							<input type="hidden" name="cellIndex" value={selectedCell} />
							<input type="hidden" name="color" value={hex} />
							<input type="hidden" name="direction" value="1" />
							<button
								type="submit"
								class="rounded px-2 py-0.5 text-sm text-green-700 hover:bg-green-100"
								aria-label="Upvote {name}">▲</button
							>
						</form>
						<form method="POST" action="?/vote" use:enhance class="contents">
							<input type="hidden" name="cellIndex" value={selectedCell} />
							<input type="hidden" name="color" value={hex} />
							<input type="hidden" name="direction" value="-1" />
							<button
								type="submit"
								class="rounded px-2 py-0.5 text-sm text-red-700 hover:bg-red-100"
								aria-label="Downvote {name}">▼</button
							>
						</form>
					</div>
				{/each}
			</div>
		</div>
	{/if}
</div>
