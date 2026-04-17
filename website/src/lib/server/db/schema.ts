import { integer, primaryKey, sqliteTable, text } from 'drizzle-orm/sqlite-core';

export const task = sqliteTable('task', {
	id: text('id')
		.primaryKey()
		.$defaultFn(() => crypto.randomUUID()),
	title: text('title').notNull(),
	priority: integer('priority').notNull().default(1)
});

export const pixelVote = sqliteTable(
	'pixel_vote',
	{
		cellIndex: integer('cell_index').notNull(),
		color: text('color').notNull(),
		score: integer('score').notNull().default(0)
	},
	(t) => [primaryKey({ columns: [t.cellIndex, t.color] })]
);

export * from './auth.schema';
