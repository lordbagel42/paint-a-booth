import { integer, sqliteTable, text } from 'drizzle-orm/sqlite-core';

export const task = sqliteTable('task', {
	id: text('id')
		.primaryKey()
		.$defaultFn(() => crypto.randomUUID()),
	title: text('title').notNull(),
	priority: integer('priority').notNull().default(1)
});

export const voteHistory = sqliteTable('vote_history', {
	id: text('id')
		.primaryKey()
		.$defaultFn(() => crypto.randomUUID()),
	userId: text('user_id').notNull(),
	cellIndex: integer('cell_index').notNull(),
	color: text('color').notNull(),
	direction: integer('direction').notNull(),
	createdAt: integer('created_at')
		.notNull()
		.$defaultFn(() => Date.now())
});

export * from './auth.schema';
