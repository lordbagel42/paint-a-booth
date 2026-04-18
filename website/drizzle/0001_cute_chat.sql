CREATE TABLE `vote_history` (
	`id` text PRIMARY KEY NOT NULL,
	`user_id` text NOT NULL,
	`cell_index` integer NOT NULL,
	`color` text NOT NULL,
	`direction` integer NOT NULL,
	`created_at` integer NOT NULL
);
